#include "global.h"
#include "nano_assets.h"
#include "nano_renderer.h"
#include "nano_resource_bridge.h"
#include <assert.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static struct nano_pack pack;
static struct nano_assets assets;
static struct nano_asset slots[512];
static struct nano_renderer renderer;
static const char *root;
static const void *ranges[32];
static size_t sizes[32];
static unsigned range_count, draws;
static struct nano_render_vertex captured[3];
static unsigned texture_checks;
static uint8_t texture[32768];
SaveContext gSaveContext;
static GameInfo game_info;
GameInfo *gGameInfo = &game_info;
static GraphicsContext graphics;
static PlayState play;
static Gfx opa[2048], xlu[2048], overlay[64];
extern void func_8009E0B8(PlayState *);

void osSyncPrintf(const char *format, ...)
{
    (void)format;
}

static void register_range(const void *p, size_t n)
{
    assert(range_count < 32);
    ranges[range_count] = p;
    sizes[range_count++] = n;
}

static const void *asset(uint64_t key, size_t n, uint32_t kind)
{
    struct nano_asset *a = nano_assets_get(&assets, key);
    if (!a || (kind && a->type != kind))
        return NULL;
    size_t size = a->bytes - ((uint8_t *)a->data - (uint8_t *)a->memory);
    return n <= size ? a->data : NULL;
}

static const void *resource(void *owner, uint64_t key, size_t n, uint32_t kind)
{
    (void)owner;
    return asset(key, n, kind);
}

static const void *address(void *owner, uintptr_t p, size_t n, uint32_t kind)
{
    (void)owner;
    size_t available = 0;
    for (unsigned i = 0; i < range_count; ++i) {
        uintptr_t start = (uintptr_t)ranges[i];
        if (p >= start && p - start < sizes[i])
            available = sizes[i] - (p - start);
    }
    for (unsigned i = 0; i < assets.capacity; ++i) {
        const struct nano_asset *a = &assets.slots[i];
        uintptr_t start = (uintptr_t)a->memory;
        if (a->ready && p >= start && p - start < a->bytes)
            available = a->bytes - (p - start);
    }
    if (kind && available >= 7 && !memcmp((void *)p, "__OTR__", 7)) {
        const char *name = (const char *)p;
        if (!memchr(name, 0, available))
            return NULL;
        return asset(nano_resource_hash(name), n, kind);
    }
    return available >= n ? (const void *)p : NULL;
}

static int triangle(void *owner, const struct nano_render_state *s,
                    const struct nano_render_vertex v[3])
{
    (void)owner;
    memcpy(captured, v, sizeof(captured));
    ++draws;
    for (unsigned i = 0; i < 3; ++i)
        for (unsigned j = 0; j < 4; ++j)
            assert(isfinite(v[i].position[j]));
    /* Check the selected texture when this list enabled texturing. The later
     * GLES combiner will decide whether either of the two tiles is sampled. */
    if (s->texture_enabled) {
        unsigned width, height;
        if (nano_renderer_texture(s, s->texture_tile, texture, sizeof(texture),
                                  &width, &height)) {
            const struct nano_render_tile *t = &s->tiles[s->texture_tile];
            fprintf(stderr,
                    "texture tile %u fmt %u size %u line %u tmem %u bounds "
                    "%g,%g,%g,%g masks %u,%u lut %x\n",
                    s->texture_tile, t->format, t->size, t->line, t->tmem,
                    t->uls, t->ult, t->lrs, t->lrt, t->masks, t->maskt,
                    s->other_h);
            return -1;
        }
        assert(width && height);
        ++texture_checks;
    }
    return 0;
}

static int clear_depth(void *owner)
{
    (void)owner;
    return 0;
}
static int invalidate(void *owner, uintptr_t p)
{
    (void)owner;
    (void)p;
    return 0;
}

static void begin(void)
{
    struct nano_render_callbacks c = {address,     resource,   triangle,
                                      clear_depth, invalidate, NULL};
    nano_renderer_init(&renderer, &c);
    renderer.state.other_h = G_TT_RGBA16;
    draws = range_count = texture_checks = 0;
}

static void synthetic(void)
{
    Vtx v[3] = {{{{-1, -1, 0}, 0, {0, 0}, {255, 0, 0, 255}}},
                {{{1, -1, 0}, 0, {32, 0}, {0, 255, 0, 255}}},
                {{{0, 1, 0}, 0, {16, 32}, {0, 0, 255, 255}}}};
    Gfx dl[8], *p = dl;
    __gSPVertex(p++, (uintptr_t)v, 3, 0);
    gSP1Triangle(p++, 0, 1, 2, 0);
    gSPEndDisplayList(p++);
    begin();
    register_range(v, sizeof(v));
    register_range(dl, (p - dl) * sizeof(*dl));
    assert(!nano_renderer_run(&renderer, dl) && draws == 1);
    assert(captured[0].position[0] == -1 && captured[2].position[1] == 1);
    assert(captured[1].shade[1] == 1 && captured[1].shade[0] == 0);
    gSP1Triangle(dl + 1, 0, 1, 4, 0);
    nano_renderer_begin(&renderer);
    assert(nano_renderer_run(&renderer, dl) &&
           renderer.error == NANO_RENDER_LIMIT);
    gSP1Triangle(dl + 1, 0, 1, 2, 0);
    nano_renderer_begin(&renderer);
    sizes[1] -= sizeof(Gfx);
    assert(nano_renderer_run(&renderer, dl) &&
           renderer.error == NANO_RENDER_ADDRESS);
    sizes[1] += sizeof(Gfx);
    __gSPDisplayList(dl, dl);
    nano_renderer_begin(&renderer);
    assert(nano_renderer_run(&renderer, dl) &&
           renderer.error == NANO_RENDER_LIMIT);
    gSPBranchList(dl, dl);
    nano_renderer_begin(&renderer);
    renderer.command_limit = 100;
    assert(nano_renderer_run(&renderer, dl) &&
           renderer.error == NANO_RENDER_LIMIT && renderer.commands == 101);
    dl[0].words.w0 = 0xab000000;
    nano_renderer_begin(&renderer);
    assert(nano_renderer_run(&renderer, dl) &&
           renderer.error == NANO_RENDER_COMMAND);

    static const uint8_t rgba[] = {0xf8, 0x01, 0x07, 0xc1,
                                   0x00, 0x3f, 0xff, 0xfe};
    Gfx texdl[20];
    p = texdl;
    gDPSetTextureImage(p++, G_IM_FMT_RGBA, G_IM_SIZ_16b, 2, rgba);
    gDPSetTile(p++, G_IM_FMT_RGBA, G_IM_SIZ_16b, 1, 0, 7, 0, 0, 0, 0, 0, 0, 0);
    gDPLoadTile(p++, 7, 0, 0, 4, 4);
    gDPSetTile(p++, G_IM_FMT_RGBA, G_IM_SIZ_16b, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0);
    gDPSetTileSize(p++, 0, 0, 0, 4, 4);
    gSPEndDisplayList(p++);
    begin();
    register_range(rgba, sizeof(rgba));
    register_range(texdl, (p - texdl) * sizeof(*texdl));
    assert(!nano_renderer_run(&renderer, texdl));
    unsigned w, h;
    assert(!nano_renderer_texture(&renderer.state, 0, texture, sizeof(texture),
                                  &w, &h) &&
           w == 2 && h == 2);
    static const uint8_t expected[] = {255, 0, 0,   255, 0,   255, 0,   255,
                                       0,   0, 255, 255, 255, 255, 255, 0};
    assert(!memcmp(texture, expected, sizeof(expected)));
    assert(nano_renderer_texture(&renderer.state, 0, texture, 15, &w, &h));
    puts("Renderer: transformed triangles, vertex/list bounds, "
         "recursion/command limits and packed RGBA16 row strides passed.");
}

static void scene_draw(unsigned frame)
{
    memset(&play, 0, sizeof(play));
    memset(&graphics, 0, sizeof(graphics));
    memset(&gSaveContext, 0, sizeof(gSaveContext));
    gSaveContext.linkAge = LINK_AGE_CHILD;
    play.state.gfxCtx = &graphics;
    play.gameplayFrames = frame;
    THGA_Ct(&graphics.polyOpa, opa, sizeof(opa));
    THGA_Ct(&graphics.polyXlu, xlu, sizeof(xlu));
    THGA_Ct(&graphics.overlay, overlay, sizeof(overlay));
    func_8009E0B8(&play);
    gSPEndDisplayList(graphics.polyOpa.p++);
    gSPEndDisplayList(graphics.polyXlu.p++);
    register_range(opa, sizeof(opa));
    register_range(xlu, sizeof(xlu));
    register_range(overlay, sizeof(overlay));
    assert(!nano_renderer_run(&renderer, opa));
    assert(!nano_renderer_run(&renderer, xlu));
}

static int read_file(void *owner, const char *name, void *out, uint32_t n,
                     uint32_t *got)
{
    (void)owner;
    char path[2048];
    snprintf(path, sizeof(path), "%s/%s", root, name);
    FILE *f = fopen(path, "rb");
    if (!f)
        return -1;
    *got = fread(out, 1, n, f);
    int error = ferror(f);
    fclose(f);
    return error ? -1 : 0;
}
static void *allocate(void *owner, size_t n)
{
    (void)owner;
    return calloc(1, n);
}
static void release(void *owner, void *p)
{
    (void)owner;
    free(p);
}

int main(int argc, char **argv)
{
    assert(argc == 3);
    synthetic();
    root = argv[1];
    assert(!nano_pack_open(&pack, read_file, NULL));
    nano_assets_init(&assets, &pack, slots, 512, 4 * 1024 * 1024, allocate,
                     release, NULL);
    assert(!nano_resources_bind(&assets));
    FILE *names = fopen(argv[2], "r");
    assert(names);
    char name[1024];
    unsigned good = 0, bad = 0;
    while (fgets(name, sizeof(name), names)) {
        name[strcspn(name, "\n")] = 0;
        const Gfx *dl =
            asset(nano_resource_hash(name), sizeof(Gfx), 0x4f444c54);
        assert(dl);
        for (unsigned frame = 0; frame < 3; ++frame) {
            const unsigned frames[] = {0, 127, 2047};
            begin();
            scene_draw(frames[frame]);
            /* The real forest draw-config supplies animated texture segments.
             * Camera/lighting remain isolated fixtures; no gameplay frame runs.
             */
            for (unsigned i = 0; i < 3; ++i)
                renderer.combined[i][i] = 0.0001f;
            int result = nano_renderer_run(&renderer, dl);
            if (result) {
                if (bad < 20)
                    fprintf(stderr,
                            "%s: error=%u w0=%08x commands=%u draws=%u "
                            "assets=%zu\n",
                            name, renderer.error, renderer.failed_w0,
                            renderer.commands, draws, assets.used);
                ++bad;
            } else
                ++good;
        }
        nano_assets_clear(&assets);
    }
    fclose(names);
    printf("External display lists: %u completed, %u rejected; isolated mesh "
           "translation, not gameplay or hardware rendering.\n",
           good, bad);
    return bad != 0;
}
