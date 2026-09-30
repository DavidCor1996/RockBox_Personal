#ifndef NANO_RENDERER_H
#define NANO_RENDERER_H
#include "libultraship/libultra.h"
#include <stddef.h>
#include <stdint.h>

/* Native F3DEX2 command translation. No N64 CPU, RSP, or RDP emulator is
 * involved. All memory access is checked by the application's range registry;
 * the backend consumes each triangle synchronously and owns its GPU copies. */
#define NANO_RENDER_VERTICES 64
#define NANO_RENDER_MATRICES 32
#define NANO_RENDER_CALLS 32
#define NANO_RENDER_TMEM 8192

enum nano_render_error {
    NANO_RENDER_OK,
    NANO_RENDER_ADDRESS,
    NANO_RENDER_COMMAND,
    NANO_RENDER_LIMIT,
    NANO_RENDER_TEXTURE,
    NANO_RENDER_BACKEND
};

struct nano_render_vertex {
    float position[4], uv[2], shade[4], fog;
    uint8_t clip, valid;
};

struct nano_render_tile {
    uint16_t tmem, line;
    float uls, ult, lrs, lrt;
    uint8_t format, size, palette, cms, cmt, masks, maskt, shifts, shiftt;
};

struct nano_render_state {
    uint32_t geometry, extra_geometry, other_l, other_h, combine[2];
    float primitive[4], environment[4], fog[4], blend[4];
    uint32_t fill;
    uint16_t primitive_depth;
    uint8_t primitive_lod, texture_tile, texture_enabled;
    float viewport[4], scissor[4]; /* Native 320 x 240 coordinates, top left. */
    struct nano_render_tile tiles[8];
    uint8_t tmem[NANO_RENDER_TMEM], valid[NANO_RENDER_TMEM / 8];
    uint8_t palette[512], palette_valid[32];
    uint32_t texture_revision;
};

struct nano_render_callbacks {
    /* Resolve direct native pointers and __OTR__ handles to a readable span.
     * The callback must reject pointers outside live registered allocations.
     * kind is 0 for raw data or the O2R resource type for a typed handle. */
    const void *(*address)(void *, uintptr_t, size_t, uint32_t kind);
    const void *(*resource)(void *, uint64_t, size_t, uint32_t kind);
    int (*triangle)(void *, const struct nano_render_state *,
                    const struct nano_render_vertex[3]);
    int (*clear_depth)(void *);
    int (*invalidate)(void *, uintptr_t);
    void *owner;
};

struct nano_renderer {
    struct nano_render_callbacks callbacks;
    struct nano_render_state state;
    struct nano_render_vertex vertices[NANO_RENDER_VERTICES];
    float modelview[NANO_RENDER_MATRICES][4][4], projection[4][4],
        combined[4][4];
    Light_t lights[8], lookat_lights[2];
    float light_direction[7][3], lookat[2][3];
    uintptr_t segments[16], image, color_image, depth_image, half1;
    uint16_t image_width, scale_s, scale_t;
    int16_t fog_multiplier, fog_offset;
    uint8_t image_size, matrix_depth, lights_count, lights_dirty;
    enum nano_render_error error;
    uint32_t commands, triangles, rejected, command_limit, failed_w0;
};

void nano_renderer_init(struct nano_renderer *,
                        const struct nano_render_callbacks *);
/* Begin resets RSP state and counters; RDP/tile state is retained as on the
 * original graphics processor. Call once before the game's first frame list. */
void nano_renderer_begin(struct nano_renderer *);
int nano_renderer_run(struct nano_renderer *, const Gfx *);
/* Convert one render tile to packed RGBA8888 in a caller-owned bounded buffer.
 * Wrap/mirror/shift are applied by the backend, not by changing source assets.
 */
int nano_renderer_texture(const struct nano_render_state *, unsigned, uint8_t *,
                          size_t, unsigned *, unsigned *);
void nano_renderer_uv(const struct nano_render_tile *, const float[2], unsigned,
                      unsigned, int, float[2]);
#endif
