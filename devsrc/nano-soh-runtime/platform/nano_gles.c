#include "nano_gles.h"
#ifdef NANO_RENDER_HOST
#define GL_GLEXT_PROTOTYPES
#include <GL/gl.h>
#else
#include "nano_gl_api.h"
/* Core texture-environment constants missing from the small SDK header. */
#define GL_COMBINE 0x8570
#define GL_COMBINE_RGB 0x8571
#define GL_COMBINE_ALPHA 0x8572
#define GL_INTERPOLATE 0x8575
#define GL_CONSTANT 0x8576
#define GL_PRIMARY_COLOR 0x8577
#define GL_PREVIOUS 0x8578
#define GL_SOURCE0_RGB 0x8580
#define GL_SOURCE0_ALPHA 0x8588
#define GL_OPERAND0_RGB 0x8590
#define GL_OPERAND0_ALPHA 0x8598
#define GL_MAX_TEXTURE_UNITS 0x84e2
#define GL_ALPHA_TEST 0x0bc0
#endif
#include <math.h>
#include <string.h>

struct stage {
    GLenum operation[2], source[2][3], operand[2][3];
    float constant[4];
    int8_t constant_source[4], texture;
};

struct vertex {
    float p[4], color[4], uv[NANO_GLES_UNITS][2];
};

static int fail(struct nano_gles *g, enum nano_gles_error e)
{
    if (!g->error)
        g->error = e;
    return -1;
}

static int check(struct nano_gles *g)
{
    GLenum e = glGetError();
    if (e) {
        g->gl_error = e;
        return fail(g, NANO_GLES_GL);
    }
    return g->error ? -1 : 0;
}

static unsigned operands(unsigned op)
{
    return op == NCC_REPLACE ? 1 : op == NCC_INTERPOLATE ? 3 : 2;
}

static float value(unsigned src, unsigned channel,
                   const struct nano_render_state *s)
{
    if (src == NCC_ONE)
        return 1;
    if (src == NCC_PRIMITIVE)
        return s->primitive[channel];
    if (src == NCC_ENVIRONMENT)
        return s->environment[channel];
    if (src == NCC_PRIMITIVE_A)
        return s->primitive[3];
    if (src == NCC_ENVIRONMENT_A)
        return s->environment[3];
    if (src == NCC_PRIMITIVE_LOD)
        return s->primitive_lod / 255.0f;
    return 0;
}

static int source(struct stage *p, unsigned ch, unsigned arg, unsigned src,
                  const struct nano_render_state *s)
{
    GLenum glsrc = GL_CONSTANT;
    int alpha = ch || src == NCC_COMBINED_A || src == NCC_TEX0_A ||
                src == NCC_TEX1_A || src == NCC_PRIMITIVE_A ||
                src == NCC_SHADE_A || src == NCC_ENVIRONMENT_A;
    if (src == NCC_COMBINED || src == NCC_COMBINED_A)
        glsrc = GL_PREVIOUS;
    else if (src == NCC_SHADE || src == NCC_SHADE_A)
        glsrc = GL_PRIMARY_COLOR;
    else if (src == NCC_TEX0 || src == NCC_TEX0_A || src == NCC_TEX1 ||
             src == NCC_TEX1_A) {
        int tex = src == NCC_TEX1 || src == NCC_TEX1_A;
        if (p->texture >= 0 && p->texture != tex)
            return -1;
        p->texture = tex;
        glsrc = GL_TEXTURE;
    } else {
        if (src == NCC_LOD || src == NCC_UNSUPPORTED)
            return -1;
        unsigned first = alpha ? 3 : 0, last = alpha ? 4 : 3;
        for (unsigned i = first; i < last; ++i) {
            if (p->constant_source[i] >= 0 && p->constant_source[i] != (int)src)
                return -1;
            p->constant_source[i] = src;
            p->constant[i] = value(src, i, s);
        }
    }
    p->source[ch][arg] = glsrc;
    p->operand[ch][arg] = alpha ? GL_SRC_ALPHA : GL_SRC_COLOR;
    return 0;
}

static int stages(struct stage output[NANO_GLES_UNITS],
                  const struct nano_cc_program *program,
                  const struct nano_render_state *s, int fog)
{
    const GLenum op[] = {GL_REPLACE, GL_MODULATE, GL_ADD, GL_INTERPOLATE};
    for (unsigned i = 0; i < program->count; ++i) {
        struct stage *p = &output[i];
        memset(p, 0, sizeof(*p));
        memset(p->constant_source, -1, sizeof(p->constant_source));
        p->texture = -1;
        for (unsigned ch = 0; ch < 2; ++ch) {
            const struct nano_cc_channel *c = &program->stage[i].channel[ch];
            p->operation[ch] = op[c->operation];
            for (unsigned j = 0; j < operands(c->operation); ++j) {
                unsigned input = c->input[j];
                if (fog && input == NCC_SHADE_A)
                    input = NCC_ONE;
                if (source(p, ch, j, input, s))
                    return -1;
            }
        }
    }
    return 0;
}

static uint64_t hash_bytes(const uint8_t *p, size_t n)
{
    uint64_t hash = UINT64_C(14695981039346656037);
    while (n--)
        hash = (hash ^ *p++) * UINT64_C(1099511628211);
    return hash;
}

static int pinned(struct nano_gles *g, uint32_t id)
{
    for (unsigned i = 0; i < NANO_GLES_UNITS; ++i)
        if (id && g->pinned[i] == id)
            return 1;
    return 0;
}

static unsigned power2(unsigned n)
{
    unsigned p = 1;
    while (p < n)
        p *= 2;
    return p;
}

static int texture(struct nano_gles *g, const struct nano_render_state *s,
                   unsigned tile, int linear, unsigned *width, unsigned *height,
                   unsigned *storage_width, unsigned *storage_height,
                   uint32_t *id)
{
    const struct nano_render_tile *t = &s->tiles[tile];
    for (unsigned i = 0; i < 2; ++i) {
        if (g->recent[i].revision != s->texture_revision ||
            g->recent[i].linear != linear ||
            memcmp(&g->recent[i].tile, t, sizeof(*t)))
            continue;
        for (unsigned j = 0; j < NANO_GLES_TEXTURES; ++j)
            if (g->textures[j].id && g->textures[j].id == g->recent[i].id) {
                g->textures[j].age = ++g->clock;
                *width = g->recent[i].width;
                *height = g->recent[i].height;
                *storage_width = g->recent[i].storage_width;
                *storage_height = g->recent[i].storage_height;
                *id = g->recent[i].id;
                return 0;
            }
    }
    unsigned w, h;
    if (nano_renderer_texture(s, tile, g->pixels, sizeof(g->pixels), &w, &h))
        return fail(g, NANO_GLES_TEXTURE);
    int mirror_s = (t->cms & G_TX_MIRROR) && !(t->cms & G_TX_CLAMP);
    int mirror_t = (t->cmt & G_TX_MIRROR) && !(t->cmt & G_TX_CLAMP);
    if (((w & (w - 1)) && !(t->cms & G_TX_CLAMP)) ||
        ((h & (h - 1)) && !(t->cmt & G_TX_CLAMP)))
        return fail(g, NANO_GLES_TEXTURE);
    unsigned sw = power2(w) * (mirror_s ? 2 : 1),
             sh = power2(h) * (mirror_t ? 2 : 1);
    if (sw > NANO_GLES_PIXELS || sh > NANO_GLES_PIXELS / sw)
        return fail(g, NANO_GLES_BUDGET);
    if (sw != w || sh != h)
        for (unsigned y = sh; y--;)
            for (unsigned x = sw; x--;) {
                unsigned sx = x < w ? x : mirror_s ? 2 * w - 1 - x : w - 1;
                unsigned sy = y < h ? y : mirror_t ? 2 * h - 1 - y : h - 1;
                memmove(g->pixels + (y * sw + x) * 4,
                        g->pixels + (sy * w + sx) * 4, 4);
            }
    uint64_t hash = hash_bytes(g->pixels, sw * sh * 4);
    uint32_t bytes = sw * sh * 4;
    unsigned selected = NANO_GLES_TEXTURES;
    for (unsigned i = 0; i < NANO_GLES_TEXTURES; ++i) {
        struct nano_gles_texture *e = &g->textures[i];
        if (e->id && e->hash == hash && e->width == sw && e->height == sh &&
            e->cms == t->cms && e->cmt == t->cmt && e->linear == linear) {
            selected = i;
            break;
        }
    }
    if (selected == NANO_GLES_TEXTURES) {
        if (bytes > g->budget)
            return fail(g, NANO_GLES_BUDGET);
        for (;;) {
            unsigned victim = NANO_GLES_TEXTURES, empty = NANO_GLES_TEXTURES;
            for (unsigned i = 0; i < NANO_GLES_TEXTURES; ++i) {
                struct nano_gles_texture *e = &g->textures[i];
                if (!e->id)
                    empty = i;
                else if (!pinned(g, e->id) &&
                         (victim == NANO_GLES_TEXTURES ||
                          e->age < g->textures[victim].age))
                    victim = i;
            }
            if (g->used + bytes <= g->budget && empty < NANO_GLES_TEXTURES) {
                selected = empty;
                break;
            }
            if (victim == NANO_GLES_TEXTURES)
                return fail(g, NANO_GLES_BUDGET);
            struct nano_gles_texture *e = &g->textures[victim];
            if (g->fence(g->owner))
                return fail(g, NANO_GLES_FENCE);
            for (unsigned i = 0; i < 2; ++i)
                if (g->recent[i].id == e->id)
                    g->recent[i].id = 0;
            glDeleteTextures(1, &e->id);
            g->used -= e->bytes;
            memset(e, 0, sizeof(*e));
        }
        struct nano_gles_texture *e = &g->textures[selected];
        glGenTextures(1, &e->id);
        glBindTexture(GL_TEXTURE_2D, e->id);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER,
                        linear ? GL_LINEAR : GL_NEAREST);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER,
                        linear ? GL_LINEAR : GL_NEAREST);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S,
                        t->cms & G_TX_CLAMP ? GL_CLAMP_TO_EDGE : GL_REPEAT);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T,
                        t->cmt & G_TX_CLAMP ? GL_CLAMP_TO_EDGE : GL_REPEAT);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, sw, sh, 0, GL_RGBA,
                     GL_UNSIGNED_BYTE, g->pixels);
        e->hash = hash;
        e->width = sw;
        e->height = sh;
        e->bytes = bytes;
        e->cms = t->cms;
        e->cmt = t->cmt;
        e->linear = linear;
        g->used += bytes;
        ++g->uploads;
        if (check(g))
            return -1;
    }
    struct nano_gles_texture *e = &g->textures[selected];
    e->age = ++g->clock;
    unsigned recent = tile & 1;
    g->recent[recent].tile = *t;
    g->recent[recent].revision = s->texture_revision;
    g->recent[recent].linear = linear;
    g->recent[recent].width = w;
    g->recent[recent].height = h;
    g->recent[recent].storage_width = sw;
    g->recent[recent].storage_height = sh;
    g->recent[recent].id = e->id;
    *width = w;
    *height = h;
    *storage_width = sw;
    *storage_height = sh;
    *id = e->id;
    return 0;
}

static void capability(GLenum cap, int on)
{
    if (on)
        glEnable(cap);
    else
        glDisable(cap);
}

int nano_gles_init(struct nano_gles *g, unsigned budget, int (*fence)(void *),
                   void *owner)
{
    memset(g, 0, sizeof(*g));
    if (!fence || !budget)
        return fail(g, NANO_GLES_FENCE);
    g->budget = budget;
    g->fence = fence;
    g->owner = owner;
    glGetIntegerv(GL_MAX_TEXTURE_UNITS, &g->units);
    if (g->units < 2)
        return fail(g, NANO_GLES_MODE);
    glGenBuffers(1, &g->buffer);
    glGenTextures(1, &g->white);
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, g->white);
    static const uint8_t white[] = {255, 255, 255, 255};
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, 1, 1, 0, GL_RGBA, GL_UNSIGNED_BYTE,
                 white);
    return check(g);
}

int nano_gles_begin(struct nano_gles *g, int x, int y, int width, int height)
{
    if (g->error || width <= 0 || height <= 0)
        return -1;
    g->x = x;
    g->y = y;
    g->width = width;
    g->height = height;
    glDisable(GL_LIGHTING);
    glDisable(GL_CULL_FACE);
    glDisable(GL_SCISSOR_TEST);
    glDisable(GL_BLEND);
    glDisable(GL_ALPHA_TEST);
    glViewport(x, y, width, height);
    glDepthMask(GL_TRUE);
    glClearColor(0, 0, 0, 1);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();
    glMatrixMode(GL_MODELVIEW);
    glLoadIdentity();
    glEnableClientState(GL_VERTEX_ARRAY);
    glEnableClientState(GL_COLOR_ARRAY);
    glDisableClientState(GL_NORMAL_ARRAY);
    glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
    return check(g);
}

int nano_gles_triangle(struct nano_gles *g, const struct nano_render_state *s,
                       const struct nano_render_vertex v[3])
{
    if (g->error)
        return -1;
    int two_cycle = (s->other_h & (3u << G_MDSFT_CYCLETYPE)) == G_CYC_2CYCLE;
    GLenum destination_blend = GL_ONE_MINUS_SRC_ALPHA;
    if (s->other_l & FORCE_BL) {
        unsigned shift = two_cycle ? 0 : 2;
        unsigned p = (s->other_l >> (28 + shift)) & 3,
                 a = (s->other_l >> (24 + shift)) & 3;
        unsigned m = (s->other_l >> (20 + shift)) & 3,
                 b = (s->other_l >> (16 + shift)) & 3;
        if (p != G_BL_CLR_IN || a != G_BL_A_IN || m != G_BL_CLR_MEM ||
            (b != G_BL_1MA && b != G_BL_1))
            return fail(g, NANO_GLES_MODE);
        if (b == G_BL_1)
            destination_blend = GL_ONE;
    }
    struct nano_cc_program program;
    struct stage p[NANO_GLES_UNITS];
    memset(g->pinned, 0, sizeof(g->pinned));
    int fog = (s->other_l >> 30) == G_BL_CLR_FOG;
    g->failed_combine[0] = s->combine[0];
    g->failed_combine[1] = s->combine[1];
    g->failed_mode = s->other_l;
    if (nano_combiner_compile(s->combine[0], s->combine[1],
                              (s->other_h & (3u << G_MDSFT_CYCLETYPE)) ==
                                  G_CYC_2CYCLE,
                              &program) ||
        stages(p, &program, s, fog))
        return fail(g, NANO_GLES_COMBINER);
    unsigned count = program.count + fog;
    if (count > (unsigned)g->units)
        return fail(g, NANO_GLES_MODE);
    if (fog) {
        struct stage *f = &p[program.count];
        memset(f, 0, sizeof(*f));
        f->texture = -1;
        f->operation[0] = GL_INTERPOLATE;
        f->operation[1] = GL_REPLACE;
        f->source[0][0] = GL_CONSTANT;
        f->source[0][1] = GL_PREVIOUS;
        f->source[0][2] = GL_PRIMARY_COLOR;
        f->operand[0][0] = f->operand[0][1] = GL_SRC_COLOR;
        f->operand[0][2] = GL_SRC_ALPHA;
        f->source[1][0] = GL_PREVIOUS;
        f->operand[1][0] = GL_SRC_ALPHA;
        memcpy(f->constant, s->fog, sizeof(f->constant));
    }
    struct vertex out[3];
    memset(out, 0, sizeof(out));
    for (unsigned i = 0; i < 3; ++i) {
        memcpy(out[i].p, v[i].position, sizeof(out[i].p));
        memcpy(out[i].color, v[i].shade, sizeof(out[i].color));
    }
    if (fog)
        for (unsigned i = 0; i < 3; ++i)
            out[i].color[3] = s->geometry & G_FOG ? v[i].fog : v[i].shade[3];
    int linear = (s->other_h & (3u << G_MDSFT_TEXTFILT)) != G_TF_POINT;
    for (unsigned i = 0; i < NANO_GLES_UNITS; ++i) {
        if (i >= (unsigned)g->units)
            break;
        glActiveTexture(GL_TEXTURE0 + i);
        glClientActiveTexture(GL_TEXTURE0 + i);
        if (i >= count) {
            glDisable(GL_TEXTURE_2D);
            glDisableClientState(GL_TEXTURE_COORD_ARRAY);
            continue;
        }
        glEnable(GL_TEXTURE_2D);
        glEnableClientState(GL_TEXTURE_COORD_ARRAY);
        uint32_t id = g->white;
        unsigned w = 1, h = 1, sw = 1, sh = 1;
        if (p[i].texture >= 0) {
            unsigned tile = (s->texture_tile + p[i].texture) & 7;
            if (texture(g, s, tile, linear, &w, &h, &sw, &sh, &id))
                return -1;
            for (unsigned j = 0; j < 3; ++j) {
                nano_renderer_uv(&s->tiles[tile], v[j].uv, w, h, linear,
                                 out[j].uv[i]);
                out[j].uv[i][0] *= (float)w / sw;
                out[j].uv[i][1] *= (float)h / sh;
            }
        }
        g->pinned[i] = id;
        glBindTexture(GL_TEXTURE_2D, id);
        glTexEnvi(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_COMBINE);
        glTexEnvfv(GL_TEXTURE_ENV, GL_TEXTURE_ENV_COLOR, p[i].constant);
        for (unsigned ch = 0; ch < 2; ++ch) {
            glTexEnvi(GL_TEXTURE_ENV, ch ? GL_COMBINE_ALPHA : GL_COMBINE_RGB,
                      p[i].operation[ch]);
            unsigned n = i < program.count
                             ? operands(program.stage[i].channel[ch].operation)
                         : ch ? 1
                              : 3;
            for (unsigned j = 0; j < n; ++j) {
                glTexEnvi(GL_TEXTURE_ENV,
                          (ch ? GL_SOURCE0_ALPHA : GL_SOURCE0_RGB) + j,
                          p[i].source[ch][j]);
                glTexEnvi(GL_TEXTURE_ENV,
                          (ch ? GL_OPERAND0_ALPHA : GL_OPERAND0_RGB) + j,
                          p[i].operand[ch][j]);
            }
        }
    }
    capability(GL_DEPTH_TEST,
               (s->geometry & G_ZBUFFER) && (s->other_l & Z_CMP));
    glDepthFunc(GL_LEQUAL);
    glDepthMask((s->other_l & Z_UPD) != 0);
    capability(GL_BLEND, (s->other_l & FORCE_BL) != 0);
    glBlendFunc(GL_SRC_ALPHA, destination_blend);
    int edge = (s->other_l & CVG_X_ALPHA) != 0;
    capability(GL_ALPHA_TEST, edge || (s->other_l & G_AC_THRESHOLD));
    glAlphaFunc(GL_GREATER, edge ? 0.3f : s->blend[3]);
    glViewport(g->x + (int)(s->viewport[0] * g->width / 320),
               g->y + g->height -
                   (int)((s->viewport[1] + s->viewport[3]) * g->height / 240),
               (int)(s->viewport[2] * g->width / 320),
               (int)(s->viewport[3] * g->height / 240));
    glEnable(GL_SCISSOR_TEST);
    glScissor(g->x + (int)(s->scissor[0] * g->width / 320),
              g->y + g->height -
                  (int)((s->scissor[1] + s->scissor[3]) * g->height / 240),
              (int)(s->scissor[2] * g->width / 320),
              (int)(s->scissor[3] * g->height / 240));
    glBindBuffer(GL_ARRAY_BUFFER, g->buffer);
    glBufferData(GL_ARRAY_BUFFER, sizeof(out), out, GL_DYNAMIC_DRAW);
    glVertexPointer(4, GL_FLOAT, sizeof(struct vertex),
                    (void *)offsetof(struct vertex, p));
    glColorPointer(4, GL_FLOAT, sizeof(struct vertex),
                   (void *)offsetof(struct vertex, color));
    for (unsigned i = 0; i < count; ++i) {
        glClientActiveTexture(GL_TEXTURE0 + i);
        glTexCoordPointer(
            2, GL_FLOAT, sizeof(struct vertex),
            (void *)(offsetof(struct vertex, uv) + i * 2 * sizeof(float)));
    }
    glDrawArrays(GL_TRIANGLES, 0, 3);
    ++g->draws;
    return check(g);
}

int nano_gles_clear_depth(struct nano_gles *g)
{
    glDisable(GL_SCISSOR_TEST);
    glDepthMask(GL_TRUE);
    glClear(GL_DEPTH_BUFFER_BIT);
    return check(g);
}

int nano_gles_invalidate(struct nano_gles *g)
{
    if (g->fence(g->owner))
        return fail(g, NANO_GLES_FENCE);
    for (unsigned i = 0; i < NANO_GLES_TEXTURES; ++i)
        if (g->textures[i].id)
            glDeleteTextures(1, &g->textures[i].id);
    memset(g->textures, 0, sizeof(g->textures));
    memset(g->recent, 0, sizeof(g->recent));
    g->used = 0;
    return check(g);
}

int nano_gles_destroy(struct nano_gles *g)
{
    if (!g->fence || g->fence(g->owner))
        return fail(g, NANO_GLES_FENCE);
    for (unsigned i = 0; i < NANO_GLES_TEXTURES; ++i)
        if (g->textures[i].id)
            glDeleteTextures(1, &g->textures[i].id);
    if (g->white)
        glDeleteTextures(1, &g->white);
    if (g->buffer)
        glDeleteBuffers(1, &g->buffer);
    memset(g, 0, sizeof(*g));
    return 0;
}
