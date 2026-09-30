#ifndef NANO_COMBINER_H
#define NANO_COMBINER_H
#include <stdint.h>

enum nano_cc_source {
    NCC_ZERO,
    NCC_ONE,
    NCC_COMBINED,
    NCC_TEX0,
    NCC_TEX1,
    NCC_PRIMITIVE,
    NCC_SHADE,
    NCC_ENVIRONMENT,
    NCC_COMBINED_A,
    NCC_TEX0_A,
    NCC_TEX1_A,
    NCC_PRIMITIVE_A,
    NCC_SHADE_A,
    NCC_ENVIRONMENT_A,
    NCC_LOD,
    NCC_PRIMITIVE_LOD,
    NCC_UNSUPPORTED
};
enum nano_cc_operation { NCC_REPLACE, NCC_MODULATE, NCC_ADD, NCC_INTERPOLATE };
struct nano_cc_channel {
    uint8_t operation, input[3];
};
struct nano_cc_stage {
    struct nano_cc_channel channel[2];
};
struct nano_cc_program {
    struct nano_cc_stage stage[2];
    uint8_t count, textures;
};

/* Preserves both independent color/alpha cycles. Unsupported equations are
 * reported, never replaced by a visually plausible but incorrect shader. */
int nano_combiner_compile(uint32_t, uint32_t, int, struct nano_cc_program *);
#endif
