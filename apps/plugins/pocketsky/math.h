#ifndef POCKETSKY_MATH_H
#define POCKETSKY_MATH_H

#include <float.h>
#include <stdint.h>

typedef double double_t;
typedef float float_t;

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif
#define M_PI_2 (M_PI / 2.0)
#define M_PI_4 (M_PI / 4.0)
#define INFINITY (__builtin_inf())
#define NAN (__builtin_nan(""))
#define isnan(value) __builtin_isnan(value)
#define isfinite(value) __builtin_isfinite(value)
#define signbit(value) __builtin_signbit(value)

double sin(double value);
double cos(double value);
double tan(double value);
double asin(double value);
double acos(double value);
double atan(double value);
double atan2(double y, double x);
double sqrt(double value);
double cbrt(double value);
double fabs(double value);
double floor(double value);
double ceil(double value);
double fmod(double value, double divisor);
double pow(double value, double exponent);
double exp(double value);
double log10(double value);
double hypot(double x, double y);
double round(double value);
double scalbn(double value, int exponent);

#endif
