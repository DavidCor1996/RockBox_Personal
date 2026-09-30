#define GL_GLEXT_PROTOTYPES
#include "nano_gles.h"
#include <EGL/egl.h>
#include <EGL/eglext.h>
#include <GL/gl.h>
#include <assert.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static struct nano_gles gpu;
static unsigned fences;
static int fence(void *p)
{
    (void)p;
    glFinish();
    ++fences;
    return glGetError() != 0;
}

static float clamp(float x)
{
    return x < 0 ? 0 : x > 1 ? 1 : x;
}
static float source(unsigned mux, unsigned column, unsigned alpha,
                    unsigned component, unsigned cycle, const float previous[4],
                    const float tex[2][4], const float prim[4],
                    const float shade[4], const float env[4])
{
    const float *colors[] = {
        previous, tex[cycle ? 1 : 0], tex[cycle ? 0 : 1], prim, shade, env};
    if (alpha) {
        if (column == 2 && (mux == 0 || mux == 6))
            return mux == 6 ? 0.6f : 0.2f;
        if (mux < 6)
            return colors[mux][3];
        return mux == 6 ? 1 : 0;
    }
    if (mux < 6)
        return colors[mux][component];
    if (column == 2) {
        if (mux >= 7 && mux <= 12)
            return colors[mux - 7][3];
        return mux == 13 ? 0.2f : mux == 14 ? 0.6f : 0;
    }
    return mux == 6 && column != 1 ? 1 : 0;
}

static void oracle(uint32_t a, uint32_t b, unsigned two,
                   const struct nano_render_state *s, const float tex[2][4],
                   const float shade[4], float out[4])
{
    const unsigned mux[2][2][4] = {
        {{(a >> 20) & 15, (b >> 28) & 15, (a >> 15) & 31, (b >> 15) & 7},
         {(a >> 12) & 7, (b >> 12) & 7, (a >> 9) & 7, (b >> 9) & 7}},
        {{(a >> 5) & 15, (b >> 24) & 15, a & 31, (b >> 6) & 7},
         {(b >> 21) & 7, (b >> 3) & 7, (b >> 18) & 7, b & 7}}};
    float previous[4] = {0};
    for (unsigned cycle = 0; cycle < 1 + two; ++cycle) {
        for (unsigned component = 0; component < 4; ++component) {
            float v[4];
            unsigned alpha = component == 3;
            for (unsigned k = 0; k < 4; ++k) {
                unsigned m = mux[cycle][alpha][k];
                if (!two) {
                    if (m == 2)
                        m = 1;
                    if (!alpha && k == 2 && m == 9)
                        m = 8;
                }
                v[k] = source(m, k, alpha, component, cycle, previous, tex,
                              s->primitive, shade, s->environment);
            }
            out[component] = clamp((v[0] - v[1]) * v[2] + v[3]);
        }
        memcpy(previous, out, sizeof(previous));
    }
}

static void setup(struct nano_render_state *s, struct nano_render_vertex v[3],
                  unsigned seed)
{
    memset(s, 0, sizeof(*s));
    memset(v, 0, 3 * sizeof(*v));
    s->viewport[2] = s->scissor[2] = 320;
    s->viewport[3] = s->scissor[3] = 240;
    s->primitive_lod = 153;
    for (unsigned i = 0; i < 4; ++i) {
        s->primitive[i] = ((seed * 31 + i * 71) % 239 + 8) / 255.0f;
        s->environment[i] = ((seed * 59 + i * 29) % 239 + 8) / 255.0f;
        for (unsigned j = 0; j < 3; ++j)
            v[j].shade[i] = ((seed * 83 + i * 61) % 239 + 8) / 255.0f;
        s->tmem[i] = (seed * 11 + i * 41) % 239 + 8;
        s->tmem[8 + i] = (seed * 47 + i * 17) % 239 + 8;
    }
    s->valid[0] = s->valid[1] = 15;
    s->texture_revision = seed;
    for (unsigned i = 0; i < 2; ++i) {
        s->tiles[i].size = 3;
        s->tiles[i].line = 1;
        s->tiles[i].tmem = i;
    }
    v[0].position[0] = -1;
    v[0].position[1] = -1;
    v[1].position[0] = 3;
    v[1].position[1] = -1;
    v[2].position[0] = -1;
    v[2].position[1] = 3;
    for (unsigned i = 0; i < 3; ++i)
        v[i].position[3] = 1;
}

static void fog_and_cache(void)
{
    struct nano_render_state s;
    struct nano_render_vertex v[3];
    setup(&s, v, 37);
    Gfx combine;
    gDPSetCombineMode(&combine, G_CC_MODULATERGBA, G_CC_MODULATERGBA2);
    s.combine[0] = combine.words.w0;
    s.combine[1] = combine.words.w1;
    s.other_h = G_CYC_2CYCLE;
    s.other_l = (uint32_t)G_BL_CLR_FOG << 30;
    s.geometry = G_FOG;
    s.fog[0] = 0.2f;
    s.fog[1] = 0.4f;
    s.fog[2] = 0.8f;
    for (unsigned i = 0; i < 3; ++i) {
        v[i].fog = 0.35f;
        v[i].shade[3] = 1;
    }
    gpu.error = 0;
    assert(!nano_gles_begin(&gpu, 0, 0, 32, 32));
    assert(!nano_gles_triangle(&gpu, &s, v));
    float tex[2][4], want[4];
    uint8_t got[4];
    for (unsigned i = 0; i < 4; ++i) {
        tex[0][i] = s.tmem[i] / 255.0f;
        tex[1][i] = s.tmem[8 + i] / 255.0f;
    }
    oracle(s.combine[0], s.combine[1], 1, &s, tex, v[0].shade, want);
    for (unsigned i = 0; i < 3; ++i)
        want[i] = want[i] * 0.65f + s.fog[i] * 0.35f;
    glReadPixels(16, 16, 1, 1, GL_RGBA, GL_UNSIGNED_BYTE, got);
    for (unsigned i = 0; i < 4; ++i)
        assert(fabsf(got[i] - want[i] * 255) < 3);
    assert(!nano_gles_invalidate(&gpu) && !gpu.used);
    gpu.budget = 4;
    gDPSetCombineLERP(&combine, TEXEL0, 0, PRIMITIVE, 0, TEXEL0, 0, PRIMITIVE,
                      0, COMBINED, 0, TEXEL0, 0, COMBINED, 0, TEXEL0, 0);
    s.combine[0] = combine.words.w0;
    s.combine[1] = combine.words.w1;
    s.other_l = 0;
    s.geometry = 0;
    assert(nano_gles_triangle(&gpu, &s, v) && gpu.error == NANO_GLES_BUDGET);
    assert(gpu.used == 4);
    unsigned live = 0;
    for (unsigned i = 0; i < NANO_GLES_TEXTURES; ++i)
        if (gpu.textures[i].id) {
            ++live;
            assert(glIsTexture(gpu.textures[i].id));
        }
    assert(live ==
           1); /* The pending triangle's first texture was not evicted. */
    gpu.error = 0;
    gpu.budget = 16;
    puts("Fog pixels and pending-draw texture pinning passed.");
}

int main(int argc, char **argv)
{
    assert(argc == 2);
    EGLDisplay display = eglGetPlatformDisplay(EGL_PLATFORM_SURFACELESS_MESA,
                                               EGL_DEFAULT_DISPLAY, NULL);
    EGLint major, minor;
    assert(display != EGL_NO_DISPLAY && eglInitialize(display, &major, &minor));
    assert(eglBindAPI(EGL_OPENGL_API));
    EGLint attributes[] = {EGL_SURFACE_TYPE,
                           EGL_PBUFFER_BIT,
                           EGL_RENDERABLE_TYPE,
                           EGL_OPENGL_BIT,
                           EGL_RED_SIZE,
                           8,
                           EGL_GREEN_SIZE,
                           8,
                           EGL_BLUE_SIZE,
                           8,
                           EGL_ALPHA_SIZE,
                           8,
                           EGL_DEPTH_SIZE,
                           24,
                           EGL_NONE};
    EGLConfig config;
    EGLint count;
    assert(eglChooseConfig(display, attributes, &config, 1, &count) &&
           count == 1);
    EGLContext context =
        eglCreateContext(display, config, EGL_NO_CONTEXT, NULL);
    EGLint pb[] = {EGL_WIDTH, 32, EGL_HEIGHT, 32, EGL_NONE};
    EGLSurface surface = eglCreatePbufferSurface(display, config, pb);
    assert(context != EGL_NO_CONTEXT && surface != EGL_NO_SURFACE &&
           eglMakeCurrent(display, surface, surface, context));
    assert(!nano_gles_init(&gpu, 16, fence, NULL));
    FILE *file = fopen(argv[1], "r");
    assert(file);
    unsigned a, b, weight, accepted = 0, rejected = 0, checks = 0,
                           mismatches = 0, accepted_weight = 0,
                           total_weight = 0;
    while (fscanf(file, "%x %x %u", &a, &b, &weight) == 3)
        for (unsigned two = 0; two < 2; ++two) {
            int unsupported = 0;
            for (unsigned seed = 1; seed <= 5; ++seed) {
                struct nano_render_state s;
                struct nano_render_vertex v[3];
                setup(&s, v, seed);
                s.combine[0] = a;
                s.combine[1] = b;
                s.other_h = two ? G_CYC_2CYCLE : G_CYC_1CYCLE;
                gpu.error = 0;
                assert(!nano_gles_begin(&gpu, 0, 0, 32, 32));
                if (nano_gles_triangle(&gpu, &s, v)) {
                    if (gpu.error != NANO_GLES_COMBINER) {
                        fprintf(stderr,
                                "Unexpected GPU error %u GL %x mux %08x %08x\n",
                                gpu.error, gpu.gl_error, a, b);
                        abort();
                    }
                    unsupported = 1;
                    break;
                }
                float tex[2][4], want[4];
                uint8_t got[4];
                for (unsigned i = 0; i < 4; ++i) {
                    tex[0][i] = s.tmem[i] / 255.0f;
                    tex[1][i] = s.tmem[8 + i] / 255.0f;
                }
                oracle(a, b, two, &s, tex, v[0].shade, want);
                glReadPixels(16, 16, 1, 1, GL_RGBA, GL_UNSIGNED_BYTE, got);
                assert(glGetError() == 0);
                for (unsigned i = 0; i < 4; ++i)
                    if (fabsf(got[i] - want[i] * 255) > 3.0f) {
                        if (mismatches < 10)
                            fprintf(stderr,
                                    "Mismatch %08x %08x two=%u seed=%u ch=%u "
                                    "got=%u expected=%.2f\n",
                                    a, b, two, seed, i, got[i], want[i] * 255);
                        ++mismatches;
                    }
                ++checks;
            }
            if (unsupported)
                ++rejected;
            else {
                ++accepted;
                accepted_weight += weight;
            }
            total_weight += weight;
        }
    fclose(file);
    assert(gpu.uploads > 32 && fences > 0 && gpu.used <= gpu.budget);
    unsigned uploads = gpu.uploads;
    fog_and_cache();
    assert(!nano_gles_destroy(&gpu));
    eglMakeCurrent(display, EGL_NO_SURFACE, EGL_NO_SURFACE, EGL_NO_CONTEXT);
    eglDestroySurface(display, surface);
    eglDestroyContext(display, context);
    eglTerminate(display);
    printf("GLES fixed-function pixel comparisons: %u, mismatches: %u; "
           "accepted mode pairs: %u; explicitly unsupported: %u; weighted "
           "coverage %u/%u; bounded uploads %u, GPU fences %u.\n",
           checks, mismatches, accepted, rejected, accepted_weight,
           total_weight, uploads, fences);
    return mismatches != 0;
}
