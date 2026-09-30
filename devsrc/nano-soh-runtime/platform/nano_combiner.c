#include "nano_combiner.h"
#include <string.h>

static unsigned color_source(unsigned value, unsigned column)
{
    static const uint8_t base[] = {NCC_COMBINED,  NCC_TEX0,  NCC_TEX1,
                                   NCC_PRIMITIVE, NCC_SHADE, NCC_ENVIRONMENT};
    static const uint8_t extra[] = {
        NCC_UNSUPPORTED + 3, NCC_COMBINED_A,     NCC_TEX0_A,        NCC_TEX1_A,
        NCC_PRIMITIVE_A,     NCC_SHADE_A,        NCC_ENVIRONMENT_A, NCC_LOD,
        NCC_PRIMITIVE_LOD,   NCC_UNSUPPORTED + 4};
    if (value < 6)
        return base[value];
    if (column == 2)
        return value < 16 ? extra[value - 6] : NCC_ZERO;
    if (value == 6 && column != 1)
        return NCC_ONE;
    if (column == 3 || value >= 8)
        return NCC_ZERO;
    return column == 0  ? NCC_UNSUPPORTED
           : value == 6 ? NCC_UNSUPPORTED + 1
                        : NCC_UNSUPPORTED + 2;
}

static unsigned alpha_source(unsigned value, unsigned column)
{
    static const uint8_t base[] = {
        NCC_COMBINED_A, NCC_TEX0_A,        NCC_TEX1_A, NCC_PRIMITIVE_A,
        NCC_SHADE_A,    NCC_ENVIRONMENT_A, NCC_ONE,    NCC_ZERO};
    if (column == 2 && value == 0)
        return NCC_LOD;
    if (column == 2 && value == 6)
        return NCC_PRIMITIVE_LOD;
    return base[value];
}

static int channel(struct nano_cc_channel *out, unsigned v[4])
{
    unsigned a = v[0], b = v[1], c = v[2], d = v[3];
    memset(out, 0, sizeof(*out));
    if (a == b || c == NCC_ZERO) {
        out->operation = NCC_REPLACE;
        out->input[0] = d;
    } else if (c == NCC_ONE && b == d) {
        out->operation = NCC_REPLACE;
        out->input[0] = a;
    } else if (b == NCC_ZERO && d == NCC_ZERO) {
        if (a == NCC_ONE || c == NCC_ONE) {
            out->operation = NCC_REPLACE;
            out->input[0] = a == NCC_ONE ? c : a;
        } else {
            out->operation = NCC_MODULATE;
            out->input[0] = a;
            out->input[1] = c;
        }
    } else if (b == d) {
        out->operation = NCC_INTERPOLATE;
        out->input[0] = a;
        out->input[1] = b;
        out->input[2] = c;
    } else if (b == NCC_ZERO && c == NCC_ONE) {
        out->operation = NCC_ADD;
        out->input[0] = a;
        out->input[1] = d;
    } else
        return -1;
    for (unsigned i = 0; i < 3; ++i)
        if (out->input[i] >= NCC_UNSUPPORTED)
            return -1;
    return 0;
}

int nano_combiner_compile(uint32_t a, uint32_t b, int two,
                          struct nano_cc_program *out)
{
    unsigned raw[2][2][4] = {
        {{(a >> 20) & 15, (b >> 28) & 15, (a >> 15) & 31, (b >> 15) & 7},
         {(a >> 12) & 7, (b >> 12) & 7, (a >> 9) & 7, (b >> 9) & 7}},
        {{(a >> 5) & 15, (b >> 24) & 15, a & 31, (b >> 6) & 7},
         {(b >> 21) & 7, (b >> 3) & 7, (b >> 18) & 7, b & 7}}};
    memset(out, 0, sizeof(*out));
    out->count = two ? 2 : 1;
    for (unsigned i = 0; i < out->count; ++i)
        for (unsigned ch = 0; ch < 2; ++ch) {
            unsigned v[4];
            for (unsigned k = 0; k < 4; ++k) {
                unsigned s = ch ? alpha_source(raw[i][ch][k], k)
                                : color_source(raw[i][ch][k], k);
                /* Match native Shipwright's convention: TEXELs exchange roles
                 * in cycle two; in one cycle TEXEL1 aliases TEXEL0. */
                if (!two) {
                    if (s == NCC_TEX1)
                        s = NCC_TEX0;
                    if (s == NCC_TEX1_A)
                        s = NCC_TEX0_A;
                } else if (i) {
                    if (s == NCC_TEX0)
                        s = NCC_TEX1;
                    else if (s == NCC_TEX1)
                        s = NCC_TEX0;
                    else if (s == NCC_TEX0_A)
                        s = NCC_TEX1_A;
                    else if (s == NCC_TEX1_A)
                        s = NCC_TEX0_A;
                }
                if (!i && (s == NCC_COMBINED || s == NCC_COMBINED_A))
                    s = NCC_ZERO;
                v[k] = s;
            }
            if (channel(&out->stage[i].channel[ch], v))
                return -1;
        }
    if (two) {
        int need_color = 0, need_alpha = 0;
        for (unsigned ch = 0; ch < 2; ++ch)
            for (unsigned j = 0; j < 3; ++j) {
                unsigned s = out->stage[1].channel[ch].input[j];
                need_color |= s == NCC_COMBINED;
                need_alpha |= s == NCC_COMBINED_A;
            }
        if (!need_color && !need_alpha) {
            out->stage[0] = out->stage[1];
            out->count = 1;
        } else {
            if (!need_color)
                memset(&out->stage[0].channel[0], 0,
                       sizeof(struct nano_cc_channel));
            if (!need_alpha)
                memset(&out->stage[0].channel[1], 0,
                       sizeof(struct nano_cc_channel));
            const struct nano_cc_stage *last = &out->stage[1];
            if (last->channel[0].operation == NCC_REPLACE &&
                last->channel[0].input[0] == NCC_COMBINED &&
                last->channel[1].operation == NCC_REPLACE &&
                last->channel[1].input[0] == NCC_COMBINED_A)
                out->count = 1;
        }
    }
    for (unsigned i = 0; i < out->count; ++i)
        for (unsigned ch = 0; ch < 2; ++ch)
            for (unsigned j = 0; j < 3; ++j) {
                unsigned s = out->stage[i].channel[ch].input[j];
                if (s == NCC_TEX0 || s == NCC_TEX0_A)
                    out->textures |= 1;
                if (s == NCC_TEX1 || s == NCC_TEX1_A)
                    out->textures |= 2;
            }
    return 0;
}
