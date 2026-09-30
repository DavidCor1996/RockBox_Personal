#ifndef NANO_GLES_H
#define NANO_GLES_H
#include "nano_combiner.h"
#include "nano_renderer.h"

#define NANO_GLES_TEXTURES 32
#define NANO_GLES_UNITS 3
#define NANO_GLES_PIXELS 8192
enum nano_gles_error {
    NANO_GLES_OK,
    NANO_GLES_GL,
    NANO_GLES_COMBINER,
    NANO_GLES_TEXTURE,
    NANO_GLES_BUDGET,
    NANO_GLES_FENCE,
    NANO_GLES_MODE
};
struct nano_gles_texture {
    uint64_t hash;
    unsigned int id;
    uint32_t age, bytes;
    uint16_t width, height;
    uint8_t cms, cmt, linear;
};
struct nano_gles {
    struct nano_gles_texture textures[NANO_GLES_TEXTURES];
    uint8_t pixels[NANO_GLES_PIXELS * 4];
    unsigned int buffer, white;
    uint32_t clock, used, budget, uploads, draws, gl_error;
    uint32_t pinned[NANO_GLES_UNITS];
    struct {
        struct nano_render_tile tile;
        uint32_t revision, id;
        unsigned width, height, storage_width, storage_height;
        int linear;
    } recent[2];
    uint32_t failed_combine[2], failed_mode;
    int x, y, width, height, units;
    enum nano_gles_error error;
    int (*fence)(void *);
    void *owner;
};

/* Calls require the app's current GLES context. The supplied fence must wait
 * for GPU completion; a renderer cache entry is never reused without it. */
int nano_gles_init(struct nano_gles *, unsigned, int (*)(void *), void *);
int nano_gles_begin(struct nano_gles *, int, int, int, int);
int nano_gles_triangle(struct nano_gles *, const struct nano_render_state *,
                       const struct nano_render_vertex[3]);
int nano_gles_clear_depth(struct nano_gles *);
int nano_gles_invalidate(struct nano_gles *);
int nano_gles_destroy(struct nano_gles *);
#endif
