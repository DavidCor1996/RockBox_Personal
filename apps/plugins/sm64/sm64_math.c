#include "sm64_rockbox.h"

/* Rockbox plugins do not link libm.  These bounded approximations are more
 * accurate than the original N64 tables need and avoid slow soft-float calls. */
float sinf(float value)
{
    const float pi = 3.14159265358979323846f;
    const float half_pi = 1.57079632679489661923f;
    const float two_pi = 6.28318530717958647692f;
    float squared;

    while (value > pi)
        value -= two_pi;
    while (value < -pi)
        value += two_pi;
    if (value > half_pi)
        value = pi - value;
    else if (value < -half_pi)
        value = -pi - value;

    squared = value * value;
    return value * (1.0f + squared *
           (-0.1666666716f + squared *
           (0.0083333477f + squared *
           (-0.0001984090f + squared *
           (0.0000027526f + squared * -0.0000000239f)))));
}

float cosf(float value)
{
    return sinf(value + 1.57079632679489661923f);
}

float sqrtf(float value)
{
    union { float f; uint32_t i; } estimate;
    int i;

    if (value <= 0.0f)
        return 0.0f;
    estimate.f = value;
    estimate.i = (estimate.i >> 1) + 0x1fc00000u;
    for (i = 0; i < 4; ++i)
        estimate.f = 0.5f * (estimate.f + value / estimate.f);
    return estimate.f;
}
