#include "nano_renderer.h"
#include <math.h>
#include <string.h>

#define TYPE_DL 0x4f444c54u
#define TYPE_VTX 0x4f415252u
#define TYPE_TEX 0x4f544558u
#define TYPE_MTX 0x4f4d5458u
#define FIELD(w, p, n) (((uint32_t)(w) >> (p)) & ((1u << (n)) - 1u))

static int fail(struct nano_renderer *r, enum nano_render_error error)
{
    if (!r->error)
        r->error = error;
    return -1;
}

static uintptr_t segmented(struct nano_renderer *r, uintptr_t p)
{
    if (p & 1u) {
        unsigned seg = p >> 24;
        uintptr_t off = p & 0xfffffeu;
        if (seg >= 16 || !r->segments[seg] ||
            r->segments[seg] > UINTPTR_MAX - off) {
            fail(r, NANO_RENDER_ADDRESS);
            return 0;
        }
        return r->segments[seg] + off;
    }
    return p;
}

static const void *address(struct nano_renderer *r, uintptr_t p, size_t n,
                           uint32_t kind)
{
    p = segmented(r, p);
    const void *result = NULL;
    if (!r->error && p)
        result = r->callbacks.address(r->callbacks.owner, p, n, kind);
    if (!result)
        fail(r, NANO_RENDER_ADDRESS);
    return result;
}

static const void *resource(struct nano_renderer *r, const Gfx *hash, size_t n,
                            uint32_t kind)
{
    uint64_t key =
        (uint64_t)(uint32_t)hash->words.w0 << 32 | (uint32_t)hash->words.w1;
    const void *result =
        r->callbacks.resource(r->callbacks.owner, key, n, kind);
    if (!result)
        fail(r, NANO_RENDER_ADDRESS);
    return result;
}

static float clamp(float v, float a, float b)
{
    return v < a ? a : v > b ? b : v;
}

static void identity(float m[4][4])
{
    memset(m, 0, 16 * sizeof(float));
    for (unsigned i = 0; i < 4; ++i)
        m[i][i] = 1;
}

static void multiply(float dst[4][4], const float a[4][4], const float b[4][4])
{
    float tmp[4][4];
    for (unsigned i = 0; i < 4; ++i)
        for (unsigned j = 0; j < 4; ++j) {
            tmp[i][j] = 0;
            for (unsigned k = 0; k < 4; ++k)
                tmp[i][j] += a[i][k] * b[k][j];
        }
    memcpy(dst, tmp, sizeof(tmp));
}

static void matrix(struct nano_renderer *r, unsigned flags, const void *p)
{
    if (!p)
        return;
    uint32_t words[16];
    float m[4][4];
    memcpy(words, p, sizeof(words));
    for (unsigned i = 0; i < 16; ++i) {
        unsigned shift = (i & 1) ? 0 : 16;
        uint32_t bits = ((words[i / 2] >> shift) & 0xffff) << 16 |
                        ((words[8 + i / 2] >> shift) & 0xffff);
        m[i / 4][i % 4] = (int32_t)bits / 65536.0f;
    }
    float (*dst)[4] = r->projection;
    if (!(flags & G_MTX_PROJECTION)) {
        if (flags & G_MTX_PUSH) {
            if (r->matrix_depth == NANO_RENDER_MATRICES) {
                fail(r, NANO_RENDER_LIMIT);
                return;
            }
            memcpy(r->modelview[r->matrix_depth],
                   r->modelview[r->matrix_depth - 1], sizeof(m));
            ++r->matrix_depth;
        }
        dst = r->modelview[r->matrix_depth - 1];
        r->lights_dirty = 1;
    }
    if (flags & G_MTX_LOAD)
        memcpy(dst, m, sizeof(m));
    else
        multiply(dst, m, dst);
    multiply(r->combined, r->modelview[r->matrix_depth - 1], r->projection);
}

static void direction(struct nano_renderer *r, const int8_t n[3], float out[3])
{
    float (*m)[4] = r->modelview[r->matrix_depth - 1];
    for (unsigned i = 0; i < 3; ++i)
        out[i] = (n[0] * m[i][0] + n[1] * m[i][1] + n[2] * m[i][2]) / 127.0f;
    float length = sqrtf(out[0] * out[0] + out[1] * out[1] + out[2] * out[2]);
    if (length > 0)
        for (unsigned i = 0; i < 3; ++i)
            out[i] /= length;
}

static void clipping(struct nano_render_vertex *v)
{
    v->clip = 0;
    for (unsigned i = 0; i < 3; ++i) {
        if (v->position[i] < -v->position[3])
            v->clip |= 1u << (i * 2);
        if (v->position[i] > v->position[3])
            v->clip |= 2u << (i * 2);
    }
}

static void vertices(struct nano_renderer *r, unsigned n, unsigned start,
                     const Vtx *src)
{
    if (!src)
        return;
    if (!n || start >= NANO_RENDER_VERTICES ||
        n > NANO_RENDER_VERTICES - start) {
        fail(r, NANO_RENDER_LIMIT);
        return;
    }
    if (r->lights_dirty) {
        for (unsigned i = 0; i < r->lights_count; ++i)
            direction(r, r->lights[i].dir, r->light_direction[i]);
        for (unsigned i = 0; i < 2; ++i)
            direction(r, r->lookat_lights[i].dir, r->lookat[i]);
        r->lights_dirty = 0;
    }
    for (unsigned i = 0; i < n; ++i) {
        const Vtx *s = &src[i];
        struct nano_render_vertex *v = &r->vertices[start + i];
        for (unsigned j = 0; j < 4; ++j)
            v->position[j] = s->v.ob[0] * r->combined[0][j] +
                             s->v.ob[1] * r->combined[1][j] +
                             s->v.ob[2] * r->combined[2][j] + r->combined[3][j];
        for (unsigned j = 0; j < 4; ++j)
            if (!isfinite(v->position[j])) {
                fail(r, NANO_RENDER_LIMIT);
                return;
            }
        v->uv[0] = s->v.tc[0] * (r->scale_s / 65536.0f);
        v->uv[1] = s->v.tc[1] * (r->scale_t / 65536.0f);
        for (unsigned j = 0; j < 4; ++j)
            v->shade[j] = s->v.cn[j] / 255.0f;
        if (r->state.geometry & G_LIGHTING) {
            for (unsigned j = 0; j < 3; ++j)
                v->shade[j] = r->lights[r->lights_count].col[j] / 255.0f;
            for (unsigned j = 0; j < r->lights_count; ++j) {
                float dot = 0;
                for (unsigned k = 0; k < 3; ++k)
                    dot += s->n.n[k] * r->light_direction[j][k] / 127.0f;
                if (dot > 0)
                    for (unsigned k = 0; k < 3; ++k)
                        v->shade[k] += dot * r->lights[j].col[k] / 255.0f;
            }
            for (unsigned k = 0; k < 3; ++k)
                v->shade[k] = clamp(v->shade[k], 0, 1);
            if (r->state.geometry & G_TEXTURE_GEN) {
                for (unsigned j = 0; j < 2; ++j) {
                    float dot = 0;
                    for (unsigned k = 0; k < 3; ++k)
                        dot += s->n.n[k] * r->lookat[j][k] / 127.0f;
                    dot = clamp(dot, -1, 1);
                    float scale = j ? r->scale_t : r->scale_s;
                    v->uv[j] = r->state.geometry & G_TEXTURE_GEN_LINEAR
                                   ? acosf(dot) * scale / 6.28318530718f
                                   : (dot + 1) * scale / 4;
                }
            }
        }
        float w = v->position[3];
        float inverse = w < 0 ? 32767.0f : 1.0f / (w < 0.001f ? 0.001f : w);
        v->fog = clamp(
            (v->position[2] * inverse * r->fog_multiplier + r->fog_offset) /
                255.0f,
            0, 1);
        if (r->state.geometry & G_FOG)
            v->shade[3] = 1;
        v->valid = 1;
        clipping(v);
    }
}

static void emit(struct nano_renderer *r, const struct nano_render_vertex v[3],
                 int cull)
{
    if (v[0].clip & v[1].clip & v[2].clip) {
        ++r->rejected;
        return;
    }
    uint32_t mode = cull ? r->state.geometry & G_CULL_BOTH : 0;
    if (r->state.extra_geometry & G_EX_INVERT_CULLING)
        mode = ((mode & G_CULL_FRONT) ? G_CULL_BACK : 0) |
               ((mode & G_CULL_BACK) ? G_CULL_FRONT : 0);
    /* Homogeneous signed area avoids division at the near plane. */
    float area = v[0].position[0] * (v[1].position[1] * v[2].position[3] -
                                     v[2].position[1] * v[1].position[3]) -
                 v[0].position[1] * (v[1].position[0] * v[2].position[3] -
                                     v[2].position[0] * v[1].position[3]) +
                 v[0].position[3] * (v[1].position[0] * v[2].position[1] -
                                     v[2].position[0] * v[1].position[1]);
    if ((mode & G_CULL_FRONT && area >= 0) ||
        (mode & G_CULL_BACK && area <= 0)) {
        ++r->rejected;
        return;
    }
    if (r->callbacks.triangle(r->callbacks.owner, &r->state, v))
        fail(r, NANO_RENDER_BACKEND);
    else
        ++r->triangles;
}

static void triangle(struct nano_renderer *r, unsigned a, unsigned b,
                     unsigned c)
{
    if (a >= NANO_RENDER_VERTICES || b >= NANO_RENDER_VERTICES ||
        c >= NANO_RENDER_VERTICES || !r->vertices[a].valid ||
        !r->vertices[b].valid || !r->vertices[c].valid) {
        fail(r, NANO_RENDER_LIMIT);
        return;
    }
    struct nano_render_vertex v[3] = {r->vertices[a], r->vertices[b],
                                      r->vertices[c]};
    emit(r, v, 1);
}

static void color(float out[4], uint32_t value)
{
    for (unsigned i = 0; i < 4; ++i)
        out[i] = ((value >> (24 - i * 8)) & 255) / 255.0f;
}

static void movemem(struct nano_renderer *r, unsigned index, unsigned offset,
                    uintptr_t p)
{
    if (index == G_MV_VIEWPORT) {
        const Vp *vp = address(r, p, sizeof(*vp), 0);
        if (!vp)
            return;
        r->state.viewport[0] = (vp->vp.vtrans[0] - vp->vp.vscale[0]) / 4.0f;
        r->state.viewport[1] = (vp->vp.vtrans[1] - vp->vp.vscale[1]) / 4.0f;
        r->state.viewport[2] = vp->vp.vscale[0] / 2.0f;
        r->state.viewport[3] = vp->vp.vscale[1] / 2.0f;
    } else if (index == G_MV_LIGHT && offset % 24 == 0) {
        unsigned n = offset / 24;
        size_t length =
            n == r->lights_count + 2u ? sizeof(Ambient_t) : sizeof(Light_t);
        const void *light = address(r, p, length, 0);
        if (!light)
            return;
        if (n < 2) {
            memcpy(&r->lookat_lights[n], light, length);
            r->lights_dirty = 1;
        } else if (n < 10) {
            memset(&r->lights[n - 2], 0, sizeof(Light_t));
            memcpy(&r->lights[n - 2], light, length);
            r->lights_dirty = 1;
        } else
            fail(r, NANO_RENDER_LIMIT);
    } else
        fail(r, NANO_RENDER_COMMAND);
}

static void moveword(struct nano_renderer *r, unsigned index, unsigned offset,
                     uintptr_t value)
{
    switch (index) {
    case G_MW_SEGMENT:
        if (offset % 4 || offset / 4 >= 16)
            fail(r, NANO_RENDER_LIMIT);
        else
            r->segments[offset / 4] = value;
        break;
    case G_MW_NUMLIGHT:
        if (value % 24 || value / 24 > 7)
            fail(r, NANO_RENDER_LIMIT);
        else {
            r->lights_count = value / 24;
            r->lights_dirty = 1;
        }
        break;
    case G_MW_FOG:
        r->fog_multiplier = value >> 16;
        r->fog_offset = value;
        break;
    case G_MW_PERSPNORM:
    case G_MW_CLIP:
        break; /* Perspective normalization is already performed in floats. */
    case G_MW_LIGHTCOL: {
        unsigned i = offset / 24;
        if (i >= 8 || (offset % 24 != 0 && offset % 24 != 4)) {
            fail(r, NANO_RENDER_COMMAND);
            break;
        }
        for (unsigned j = 0; j < 3; ++j)
            r->lights[i].col[j] = value >> (24 - 8 * j);
        break;
    }
    default:
        fail(r, NANO_RENDER_COMMAND);
        break;
    }
}

static void mark(uint8_t *bits, unsigned offset, unsigned size)
{
    while (size--) {
        bits[offset / 8] |= 1u << (offset % 8);
        ++offset;
    }
}

static int valid(const uint8_t *bits, unsigned offset, unsigned size)
{
    while (size--) {
        if (!(bits[offset / 8] & (1u << (offset % 8))))
            return 0;
        ++offset;
    }
    return 1;
}

static void load(struct nano_renderer *r, uint32_t w0, uint32_t w1,
                 unsigned opcode)
{
    unsigned tile = FIELD(w1, 24, 3), x = FIELD(w0, 12, 12) / 4,
             y = FIELD(w0, 0, 12) / 4;
    struct nano_render_tile *t = &r->state.tiles[tile];
    unsigned bits = 4u << r->image_size;
    unsigned stride = (r->image_width * bits + 7) / 8;
    unsigned width = FIELD(w1, 12, 12) / 4 + 1;
    unsigned height = FIELD(w1, 0, 12) / 4 + 1;
    unsigned dest = t->tmem * 8;
    if (opcode == G_LOADTLUT) {
        unsigned n = FIELD(w1, 14, 10) + 1;
        if (t->tmem < 256 || t->tmem >= 512 || n > 512u - t->tmem ||
            r->image_size != 2) {
            fail(r, NANO_RENDER_TEXTURE);
            return;
        }
        const void *src = address(r, r->image, n * 2, TYPE_TEX);
        if (!src)
            return;
        memcpy(r->state.palette + (t->tmem - 256) * 2, src, n * 2);
        mark(r->state.palette_valid, t->tmem - 256, n);
    } else if (opcode == G_LOADBLOCK) {
        unsigned n = ((FIELD(w1, 12, 12) + 1) * bits + 7) / 8;
        if (x || y || dest >= NANO_RENDER_TMEM || n > NANO_RENDER_TMEM - dest) {
            fail(r, NANO_RENDER_TEXTURE);
            return;
        }
        const void *src = address(r, r->image, n, TYPE_TEX);
        if (!src)
            return;
        memcpy(r->state.tmem + dest, src, n);
        mark(r->state.valid, dest, n);
    } else {
        if (width <= x || height <= y || width > r->image_width ||
            (x * bits) % 8) {
            fail(r, NANO_RENDER_TEXTURE);
            return;
        }
        width -= x;
        height -= y;
        unsigned row = (width * bits + 7) / 8;
        unsigned pitch = t->line * (r->image_size == 3 ? 16 : 8);
        if (!pitch)
            pitch = row;
        if (row > pitch || dest >= NANO_RENDER_TMEM ||
            (height - 1) * pitch + row > NANO_RENDER_TMEM - dest) {
            fail(r, NANO_RENDER_TEXTURE);
            return;
        }
        size_t offset = y * stride + x * bits / 8;
        const uint8_t *src = address(
            r, r->image, offset + (height - 1) * stride + row, TYPE_TEX);
        if (!src)
            return;
        for (unsigned i = 0; i < height; ++i) {
            memcpy(r->state.tmem + dest + i * pitch, src + offset + i * stride,
                   row);
            mark(r->state.valid, dest + i * pitch, row);
        }
    }
    ++r->state.texture_revision;
}

static void rectangle(struct nano_renderer *r, uint32_t w0, uint32_t w1,
                      uint32_t st, uint32_t step, unsigned op)
{
    float x0 = FIELD(w1, 12, 12) / 4.0f, y0 = FIELD(w1, 0, 12) / 4.0f;
    float x1 = FIELD(w0, 12, 12) / 4.0f, y1 = FIELD(w0, 0, 12) / 4.0f;
    unsigned cycle = FIELD(r->state.other_h, G_MDSFT_CYCLETYPE, 2);
    if (cycle >= 2) {
        x1 += 1;
        y1 += 1;
    }
    if (x1 <= x0 || y1 <= y0)
        return;
    if (op == G_FILLRECT && r->depth_image &&
        r->color_image == r->depth_image) {
        if (x0 != 0 || y0 != 0 || x1 != 320 || y1 != 240) {
            fail(r, NANO_RENDER_COMMAND);
            return;
        }
        if (r->callbacks.clear_depth(r->callbacks.owner))
            fail(r, NANO_RENDER_BACKEND);
        return;
    }
    float old_view[4];
    memcpy(old_view, r->state.viewport, sizeof(old_view));
    uint32_t old_geometry = r->state.geometry, old_h = r->state.other_h;
    uint32_t old_combine[2] = {r->state.combine[0], r->state.combine[1]};
    uint8_t old_tile = r->state.texture_tile;
    r->state.viewport[0] = r->state.viewport[1] = 0;
    r->state.viewport[2] = 320;
    r->state.viewport[3] = 240;
    r->state.geometry = 0;
    r->state.texture_tile = FIELD(w1, 24, 3);
    struct nano_render_vertex v[4] = {0};
    float u = (int16_t)(st >> 16), t = (int16_t)st;
    float du = (int16_t)(step >> 16) / 32.0f, dv = (int16_t)step / 32.0f;
    if (cycle == 2)
        du /= 4;
    for (unsigned i = 0; i < 4; ++i) {
        float x = i & 1 ? x1 : x0, y = i & 2 ? y1 : y0;
        v[i].position[0] = x / 160 - 1;
        v[i].position[1] = 1 - y / 120;
        v[i].position[2] = r->state.other_l & G_ZS_PRIM
                               ? r->state.primitive_depth / 16384.0f - 1
                               : -1;
        v[i].position[3] = 1;
        v[i].valid = 1;
        v[i].uv[0] = u + (op == G_TEXRECTFLIP ? y - y0 : x - x0) * du;
        v[i].uv[1] = t + (op == G_TEXRECTFLIP ? x - x0 : y - y0) * dv;
        for (unsigned j = 0; j < 4; ++j)
            v[i].shade[j] = 1;
        if (op == G_FILLRECT) {
            uint16_t f = r->state.fill;
            v[i].shade[0] = (f >> 11) / 31.0f;
            v[i].shade[1] = ((f >> 6) & 31) / 31.0f;
            v[i].shade[2] = ((f >> 1) & 31) / 31.0f;
            v[i].shade[3] = f & 1;
        }
    }
    if (op == G_FILLRECT || cycle == 2) {
        /* Standard SHADE / DECALRGBA mux; both cycles are identical. */
        Gfx combine;
        if (op == G_FILLRECT)
            gDPSetCombineMode(&combine, G_CC_SHADE, G_CC_SHADE);
        else
            gDPSetCombineMode(&combine, G_CC_DECALRGBA, G_CC_DECALRGBA);
        r->state.combine[0] = combine.words.w0;
        r->state.combine[1] = combine.words.w1;
        r->state.other_h &= ~(3u << G_MDSFT_CYCLETYPE);
    }
    const struct nano_render_vertex a[3] = {v[0], v[1], v[2]},
                                    b[3] = {v[1], v[3], v[2]};
    emit(r, a, 0);
    if (!r->error)
        emit(r, b, 0);
    memcpy(r->state.viewport, old_view, sizeof(old_view));
    memcpy(r->state.combine, old_combine, sizeof(old_combine));
    r->state.geometry = old_geometry;
    r->state.other_h = old_h;
    r->state.texture_tile = old_tile;
}

void nano_renderer_begin(struct nano_renderer *r)
{
    r->error = NANO_RENDER_OK;
    r->commands = r->triangles = r->rejected = r->failed_w0 = 0;
    r->matrix_depth = 1;
    r->lights_count = 1;
    r->lights_dirty = 1;
    memset(r->vertices, 0, sizeof(r->vertices));
    memset(r->segments, 0, sizeof(r->segments));
    identity(r->modelview[0]);
    identity(r->projection);
    identity(r->combined);
    r->scale_s = r->scale_t = 0xffff;
}

void nano_renderer_init(struct nano_renderer *r,
                        const struct nano_render_callbacks *callbacks)
{
    memset(r, 0, sizeof(*r));
    r->callbacks = *callbacks;
    r->command_limit = 100000;
    r->lookat_lights[0].dir[0] = 127;
    r->lookat_lights[1].dir[1] = 127;
    r->state.viewport[2] = r->state.scissor[2] = 320;
    r->state.viewport[3] = r->state.scissor[3] = 240;
    nano_renderer_begin(r);
}

int nano_renderer_run(struct nano_renderer *r, const Gfx *start)
{
    uintptr_t pc = (uintptr_t)start, stack[NANO_RENDER_CALLS];
    unsigned depth = 0;
    while (!r->error) {
        if (++r->commands > r->command_limit)
            return fail(r, NANO_RENDER_LIMIT);
        const Gfx *cmd = address(r, pc, sizeof(Gfx), TYPE_DL);
        if (!cmd)
            break;
        uint32_t w0 = cmd->words.w0, w1 = cmd->words.w1;
        uintptr_t pointer = cmd->words.w1;
        unsigned op = w0 >> 24, n = FIELD(w0, 12, 8),
                 index = FIELD(w0, 1, 7) - n;
        r->failed_w0 = w0;
        pc = (uintptr_t)cmd + sizeof(Gfx);
        const Gfx *extra = NULL;
        if (op == G_SETTIMG_OTR_HASH || op == G_DL_OTR_HASH ||
            op == G_VTX_OTR_HASH || op == G_MARKER || op == G_BRANCH_Z_OTR ||
            op == G_MTX_OTR || op == G_MOVEMEM_OTR) {
            extra = address(r, pc, sizeof(Gfx), TYPE_DL);
            if (!extra)
                break;
            pc += sizeof(Gfx);
        }
        switch (op) {
        case G_NOOP:
        case G_SPNOOP:
        case G_RDPPIPESYNC:
        case G_RDPLOADSYNC:
        case G_RDPTILESYNC:
        case G_RDPFULLSYNC:
        case G_MARKER:
            break;
        case G_MTX:
        case G_MTX_OTR:
        case G_MTX_OTR_FILEPATH:
            matrix(r, FIELD(w0, 0, 8) ^ G_MTX_PUSH,
                   extra ? resource(r, extra, 64, TYPE_MTX)
                         : address(r, pointer, 64, TYPE_MTX));
            break;
        case G_POPMTX:
            n = w1 / 64;
            if (w1 % 64 || n >= r->matrix_depth) {
                fail(r, NANO_RENDER_LIMIT);
                break;
            }
            r->matrix_depth -= n;
            multiply(r->combined, r->modelview[r->matrix_depth - 1],
                     r->projection);
            r->lights_dirty = 1;
            break;
        case G_MOVEMEM:
            movemem(r, FIELD(w0, 0, 8), FIELD(w0, 8, 8) * 8, pointer);
            break;
        case G_MOVEWORD:
            moveword(r, FIELD(w0, 16, 8), FIELD(w0, 0, 16), pointer);
            break;
        case G_TEXTURE:
            r->scale_s = w1 >> 16;
            r->scale_t = w1;
            r->state.texture_tile = FIELD(w0, 8, 3);
            r->state.texture_enabled = FIELD(w0, 1, 7);
            break;
        case G_VTX:
        case G_VTX_OTR_HASH: {
            if (n > NANO_RENDER_VERTICES || index >= NANO_RENDER_VERTICES ||
                n > NANO_RENDER_VERTICES - index) {
                fail(r, NANO_RENDER_LIMIT);
                break;
            }
            const uint8_t *p;
            if (extra) {
                if (pointer > 0xfffff || pointer % sizeof(Vtx)) {
                    fail(r, NANO_RENDER_ADDRESS);
                    break;
                }
                p = resource(r, extra, pointer + n * sizeof(Vtx), TYPE_VTX);
                if (p)
                    p += pointer;
            } else
                p = address(r, pointer, n * sizeof(Vtx), TYPE_VTX);
            vertices(r, n, index, (const Vtx *)p);
            break;
        }
        case G_MODIFYVTX: {
            index = FIELD(w0, 1, 15);
            n = FIELD(w0, 16, 8);
            if (index >= NANO_RENDER_VERTICES || !r->vertices[index].valid) {
                fail(r, NANO_RENDER_LIMIT);
                break;
            }
            struct nano_render_vertex *v = &r->vertices[index];
            if (n == G_MWO_POINT_RGBA)
                color(v->shade, w1);
            else if (n == G_MWO_POINT_ST) {
                v->uv[0] = (int16_t)(w1 >> 16);
                v->uv[1] = (int16_t)w1;
            } else
                fail(r, NANO_RENDER_COMMAND);
            break;
        }
        case G_DL:
        case G_DL_OTR_HASH:
        case G_DL_OTR_FILEPATH:
        case G_DL_INDEX:
            if (op == G_DL_INDEX) {
                unsigned seg = w1 >> 24, off = w1 & 0xffffff;
                if (seg >= 16 || off > 0xfffffe / sizeof(Gfx)) {
                    fail(r, NANO_RENDER_ADDRESS);
                    break;
                }
                pointer = (seg << 24) | (off * sizeof(Gfx)) | 1;
            }
            start = extra ? resource(r, extra, sizeof(Gfx), TYPE_DL)
                          : address(r, pointer, sizeof(Gfx), TYPE_DL);
            if (!(w0 & 0x10000)) {
                if (depth == NANO_RENDER_CALLS) {
                    fail(r, NANO_RENDER_LIMIT);
                    break;
                }
                stack[depth++] = pc;
            }
            pc = (uintptr_t)start;
            break;
        case G_ENDDL:
            if (!depth) {
                r->failed_w0 = 0;
                return 0;
            }
            pc = stack[--depth];
            break;
        case G_CULLDL: {
            unsigned first = FIELD(w0, 1, 15), last = FIELD(w1, 1, 15),
                     clip = 63;
            if (last < first || last >= NANO_RENDER_VERTICES) {
                fail(r, NANO_RENDER_LIMIT);
                break;
            }
            for (unsigned i = first; i <= last; ++i) {
                if (!r->vertices[i].valid) {
                    fail(r, NANO_RENDER_LIMIT);
                    break;
                }
                clip &= r->vertices[i].clip;
            }
            if (clip) {
                if (!depth)
                    return r->error ? -1 : 0;
                pc = stack[--depth];
            }
            break;
        }
        case G_BRANCH_Z_OTR:
            index = w0 & 0xfff;
            if (index >= NANO_RENDER_VERTICES || !r->vertices[index].valid) {
                fail(r, NANO_RENDER_LIMIT);
                break;
            }
            if (r->vertices[index].position[2] <= (int32_t)w1 ||
                r->state.extra_geometry & G_EX_ALWAYS_EXECUTE_BRANCH)
                pc = (uintptr_t)resource(r, extra, sizeof(Gfx), TYPE_DL);
            break;
        case G_GEOMETRYMODE:
            r->state.geometry = (r->state.geometry & (w0 & 0xffffff)) | w1;
            break;
        case G_EXTRAGEOMETRYMODE:
            r->state.extra_geometry =
                (r->state.extra_geometry & ~(w0 & 0xffffff)) | w1;
            break;
        case G_TRI1:
        case G_TRI1_OTR:
            triangle(r, FIELD(w0, 16, 8) / 2, FIELD(w0, 8, 8) / 2,
                     FIELD(w0, 0, 8) / 2);
            break;
        case G_TRI2:
        case G_QUAD:
            triangle(r, FIELD(w0, 16, 8) / 2, FIELD(w0, 8, 8) / 2,
                     FIELD(w0, 0, 8) / 2);
            if (!r->error)
                triangle(r, FIELD(w1, 16, 8) / 2, FIELD(w1, 8, 8) / 2,
                         FIELD(w1, 0, 8) / 2);
            break;
        case G_SETOTHERMODE_L:
        case G_SETOTHERMODE_H: {
            unsigned bits = FIELD(w0, 0, 8) + 1, top = FIELD(w0, 8, 8);
            if (bits > 32 || top > 32 - bits) {
                fail(r, NANO_RENDER_COMMAND);
                break;
            }
            unsigned shift = 32 - top - bits;
            /* Pinned Torch OoTDListHelpers::ExportOpcodeFixups stores the
             * two-bit LUT value unshifted. Runtime gbi.h commands retain the
             * original shifted encoding; accept both without altering lists. */
            if (op == G_SETOTHERMODE_H && shift == G_MDSFT_TEXTLUT &&
                bits == 2 && w1 < 4)
                w1 <<= G_MDSFT_TEXTLUT;
            uint32_t mask = (UINT32_MAX >> (32 - bits)) << shift;
            uint32_t *mode =
                op == G_SETOTHERMODE_L ? &r->state.other_l : &r->state.other_h;
            *mode = (*mode & ~mask) | (w1 & mask);
            break;
        }
        case G_RDPSETOTHERMODE:
            r->state.other_h = w0 & 0xffffff;
            r->state.other_l = w1;
            break;
        case G_SETTIMG:
        case G_SETTIMG_OTR_HASH:
        case G_SETTIMG_OTR_FILEPATH:
            r->image = (uintptr_t)(extra ? resource(r, extra, 1, TYPE_TEX)
                                         : address(r, pointer, 1, TYPE_TEX));
            r->image_width = FIELD(w0, 0, 12) + 1;
            r->image_size = FIELD(w0, 19, 2);
            break;
        case G_SETTILE: {
            struct nano_render_tile *t = &r->state.tiles[FIELD(w1, 24, 3)];
            t->format = FIELD(w0, 21, 3);
            t->size = FIELD(w0, 19, 2);
            t->line = FIELD(w0, 9, 9);
            t->tmem = FIELD(w0, 0, 9);
            t->palette = FIELD(w1, 20, 4);
            t->cmt = FIELD(w1, 18, 2);
            t->cms = FIELD(w1, 8, 2);
            t->maskt = FIELD(w1, 14, 4);
            t->masks = FIELD(w1, 4, 4);
            t->shiftt = FIELD(w1, 10, 4);
            t->shifts = FIELD(w1, 0, 4);
            break;
        }
        case G_SETTILESIZE: {
            struct nano_render_tile *t = &r->state.tiles[FIELD(w1, 24, 3)];
            t->uls = FIELD(w0, 12, 12);
            t->ult = FIELD(w0, 0, 12);
            t->lrs = FIELD(w1, 12, 12);
            t->lrt = FIELD(w1, 0, 12);
            break;
        }
        case G_SETTILESIZE_INTERP:
        case G_SETTILESIZE_LERP: {
            unsigned parts = op == G_SETTILESIZE_LERP ? 4 : 2;
            extra = address(r, pc, parts * sizeof(Gfx), TYPE_DL);
            if (!extra)
                break;
            float coords[4];
            for (unsigned i = 0; i < 4; ++i) {
                uint32_t bits =
                    i & 1 ? extra[i / 2].words.w1 : extra[i / 2].words.w0;
                memcpy(&coords[i], &bits, sizeof(bits));
                if (!isfinite(coords[i]) || fabsf(coords[i]) > 65536)
                    fail(r, NANO_RENDER_TEXTURE);
            }
            /* Run at native game cadence: use this tick's coordinates, with
             * no extra interpolated frame between this and the next tick. */
            struct nano_render_tile *t = &r->state.tiles[FIELD(w1, 24, 3)];
            t->uls = coords[0];
            t->ult = coords[1];
            t->lrs = coords[2];
            t->lrt = coords[3];
            pc += parts * sizeof(Gfx);
            break;
        }
        case G_LOADBLOCK:
        case G_LOADTILE:
        case G_LOADTLUT:
            load(r, w0, w1, op);
            break;
        case G_SETCOMBINE:
            r->state.combine[0] = w0;
            r->state.combine[1] = w1;
            break;
        case G_SETPRIMCOLOR:
            color(r->state.primitive, w1);
            r->state.primitive_lod = w0;
            break;
        case G_SETENVCOLOR:
            color(r->state.environment, w1);
            break;
        case G_SETFOGCOLOR:
            color(r->state.fog, w1);
            break;
        case G_SETBLENDCOLOR:
            color(r->state.blend, w1);
            break;
        case G_SETFILLCOLOR:
            r->state.fill = w1;
            break;
        case G_SETPRIMDEPTH:
            r->state.primitive_depth = (w1 >> 16) & 0x7fff;
            break;
        case G_SETSCISSOR:
            r->state.scissor[0] = FIELD(w0, 12, 12) / 4.0f;
            r->state.scissor[1] = FIELD(w0, 0, 12) / 4.0f;
            r->state.scissor[2] =
                FIELD(w1, 12, 12) / 4.0f - r->state.scissor[0];
            r->state.scissor[3] = FIELD(w1, 0, 12) / 4.0f - r->state.scissor[1];
            break;
        case G_SETZIMG:
            r->depth_image = segmented(r, pointer);
            break;
        case G_SETCIMG:
            r->color_image = segmented(r, pointer);
            break;
        case G_FILLRECT:
            rectangle(r, w0, w1, 0, 0, op);
            break;
        case G_TEXRECT:
        case G_TEXRECTFLIP:
            extra = address(r, pc, 2 * sizeof(Gfx), TYPE_DL);
            if (!extra)
                break;
            if ((extra[0].words.w0 >> 24) != G_RDPHALF_1 ||
                (extra[1].words.w0 >> 24) != G_RDPHALF_2) {
                fail(r, NANO_RENDER_COMMAND);
                break;
            }
            rectangle(r, w0, w1, extra[0].words.w1, extra[1].words.w1, op);
            pc += 2 * sizeof(Gfx);
            break;
        case G_INVALTEXCACHE:
            if (r->callbacks.invalidate(r->callbacks.owner, pointer))
                fail(r, NANO_RENDER_BACKEND);
            break;
        default:
            fail(r, NANO_RENDER_COMMAND);
            break;
        }
    }
    return -1;
}

static void rgba16(uint8_t out[4], unsigned color16)
{
    out[0] = (color16 >> 11) * 255 / 31;
    out[1] = ((color16 >> 6) & 31) * 255 / 31;
    out[2] = ((color16 >> 1) & 31) * 255 / 31;
    out[3] = color16 & 1 ? 255 : 0;
}

int nano_renderer_texture(const struct nano_render_state *s, unsigned tile,
                          uint8_t *out, size_t capacity, unsigned *width,
                          unsigned *height)
{
    if (tile >= 8)
        return -1;
    const struct nano_render_tile *t = &s->tiles[tile];
    if (t->lrs < t->uls || t->lrt < t->ult)
        return -1;
    float fw = (t->lrs - t->uls) / 4 + 1, fh = (t->lrt - t->ult) / 4 + 1;
    if (!isfinite(fw) || !isfinite(fh) || fw < 1 || fh < 1 || fw > 4096 ||
        fh > 4096)
        return -1;
    unsigned w = fw, h = fh;
    if (t->masks && (1u << t->masks) < w)
        w = 1u << t->masks;
    if (t->maskt && (1u << t->maskt) < h)
        h = 1u << t->maskt;
    if (w > capacity / 4 || h > capacity / (w * 4))
        return -1;
    unsigned pitch = t->line * (t->size == 3 ? 16 : 8), base = t->tmem * 8;
    unsigned bits = 4u << t->size;
    if (!pitch)
        pitch = (w * bits + 7) / 8;
    if ((w * bits + 7) / 8 > pitch)
        return -1;
    for (unsigned y = 0; y < h; ++y)
        for (unsigned x = 0; x < w; ++x) {
            unsigned off = base + y * pitch + x * bits / 8, n = (bits + 7) / 8;
            if (off >= NANO_RENDER_TMEM || n > NANO_RENDER_TMEM - off ||
                !valid(s->valid, off, n))
                return -1;
            const uint8_t *p = s->tmem + off;
            uint8_t *d = out + (y * w + x) * 4;
            unsigned b = t->size == 0 ? (*p >> ((x & 1) ? 0 : 4)) & 15 : *p;
            if (t->format == G_IM_FMT_RGBA && t->size == 2)
                rgba16(d, p[0] * 256 + p[1]);
            else if (t->format == G_IM_FMT_RGBA && t->size == 3)
                memcpy(d, p, 4);
            else if (t->format == G_IM_FMT_CI && t->size <= 1) {
                if (!t->size)
                    b += t->palette * 16;
                if (!valid(s->palette_valid, b, 1))
                    return -1;
                p = s->palette + b * 2;
                unsigned lut = s->other_h & (3u << G_MDSFT_TEXTLUT);
                if (lut == G_TT_RGBA16)
                    rgba16(d, p[0] * 256 + p[1]);
                else if (lut == G_TT_IA16) {
                    d[0] = d[1] = d[2] = p[0];
                    d[3] = p[1];
                } else
                    return -1;
            } else if (t->format == G_IM_FMT_IA && t->size <= 2) {
                d[0] = d[1] = d[2] = t->size == 0   ? (b >> 1) * 255 / 7
                                     : t->size == 1 ? (b >> 4) * 17
                                                    : p[0];
                d[3] = t->size == 0   ? (b & 1) * 255
                       : t->size == 1 ? (b & 15) * 17
                                      : p[1];
            } else if (t->format == G_IM_FMT_I && t->size <= 1) {
                d[0] = d[1] = d[2] = d[3] = t->size ? b : b * 17;
            } else
                return -1;
        }
    *width = w;
    *height = h;
    return 0;
}

void nano_renderer_uv(const struct nano_render_tile *t, const float in[2],
                      unsigned w, unsigned h, int linear, float out[2])
{
    unsigned shift[2] = {t->shifts, t->shiftt};
    float start[2] = {t->uls, t->ult};
    unsigned size[2] = {w, h};
    for (unsigned i = 0; i < 2; ++i) {
        float scale = shift[i] <= 10 ? 1.0f / (1u << shift[i])
                                     : (float)(1u << (16 - shift[i]));
        out[i] = (in[i] / 32 * scale - start[i] / 4.0f + (linear ? 0.5f : 0)) /
                 size[i];
    }
}
