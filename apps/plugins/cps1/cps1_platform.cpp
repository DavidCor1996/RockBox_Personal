#include "cps1.h"
extern "C" {
#include <tlsf.h>
}
#include <stdarg.h>

/* The imported core uses libc-shaped macros; this bridge uses rb directly. */
#undef memset

size_t cps1_wcslen(const wchar_t *value)
{
    const wchar_t *cursor = value;

    while (*cursor)
        cursor++;
    return (size_t)(cursor - value);
}

size_t cps1_wcstombs(char *output, const wchar_t *input, size_t size)
{
    size_t index = 0;

    if (!size)
        return 0;
    while (input[index] && index + 1 < size)
    {
        wchar_t value = input[index];
        output[index] = value >= 0 && value <= 0x7f ? (char)value : '?';
        index++;
    }
    output[index] = '\0';
    return index;
}

static bool allocation_failed;
static size_t last_allocation_size;

int bBurnUseRomCache;

unsigned int BurnCacheBlockSize(int)
{
    return 0;
}

int BurnCacheRead(unsigned char *, int)
{
    return 1;
}

void UpperFree(void *memory)
{
    cps1_free(memory);
}

/* CPS2 drivers are deliberately absent from this CPS1-only build. */
void cps2_decrypt_game_data(void)
{
}

void *cps1_malloc(size_t size)
{
    last_allocation_size = size;
    void *result = tlsf_malloc(size ? size : 1);

    if (!result)
        allocation_failed = true;
    return result;
}

void *cps1_calloc(size_t count, size_t size)
{
    void *result;

    if (count && size > (size_t)-1 / count)
        return NULL;
    result = cps1_malloc(count * size);
    if (result)
        rb->memset(result, 0, count * size);
    return result;
}

void *cps1_realloc(void *pointer, size_t size)
{
    last_allocation_size = size;
    void *result = tlsf_realloc(pointer, size);

    if (!result && size)
        allocation_failed = true;
    return result;
}

void cps1_free(void *pointer)
{
    if (pointer)
        tlsf_free(pointer);
}

void cps1_abort(void)
{
    allocation_failed = true;
}

int cps1_printf(const char *format, ...)
{
    va_list ap;
    char line[192];
    int result;

    va_start(ap, format);
    result = rb->vsnprintf(line, sizeof(line), format, ap);
    va_end(ap);
    DEBUGF("cps1: %s", line);
    return result;
}

int cps1_sprintf(char *output, const char *format, ...)
{
    va_list ap;
    int result;

    va_start(ap, format);
    result = rb->vsnprintf(output, MAX_PATH, format, ap);
    va_end(ap);
    return result;
}

double cps1_floor(double value)
{
    long integer = (long)value;

    if ((double)integer > value)
        integer--;
    return (double)integer;
}

double cps1_sqrt(double value)
{
    double estimate;
    int iteration;

    if (value <= 0.0)
        return 0.0;
    estimate = value > 1.0 ? value : 1.0;
    for (iteration = 0; iteration < 12; iteration++)
        estimate = (estimate + value / estimate) * 0.5;
    return estimate;
}

double cps1_sin(double value)
{
    const double pi = 3.14159265358979323846;
    const double two_pi = 6.28318530717958647692;
    double square;
    double term;
    double result;
    int index;

    while (value > pi)
        value -= two_pi;
    while (value < -pi)
        value += two_pi;
    if (value > pi * 0.5)
        value = pi - value;
    else if (value < pi * -0.5)
        value = -pi - value;
    square = value * value;
    term = value;
    result = value;
    for (index = 1; index < 10; index++)
    {
        term *= -square / ((index * 2.0) * (index * 2.0 + 1.0));
        result += term;
    }
    return result;
}

double cps1_log(double value)
{
    const double ln2 = 0.69314718055994530942;
    double y;
    double y2;
    double term;
    double result;
    int exponent = 0;
    int index;

    if (value <= 0.0)
        return -1.0e30;
    while (value >= 2.0)
    {
        value *= 0.5;
        exponent++;
    }
    while (value < 1.0)
    {
        value *= 2.0;
        exponent--;
    }
    y = (value - 1.0) / (value + 1.0);
    y2 = y * y;
    term = y;
    result = 0.0;
    for (index = 1; index < 24; index += 2)
    {
        result += term / index;
        term *= y2;
    }
    return result * 2.0 + exponent * ln2;
}

static double cps1_exp(double value)
{
    const double ln2 = 0.69314718055994530942;
    double term = 1.0;
    double result = 1.0;
    int exponent = 0;
    int index;

    while (value > ln2)
    {
        value -= ln2;
        exponent++;
    }
    while (value < -ln2)
    {
        value += ln2;
        exponent--;
    }
    for (index = 1; index < 24; index++)
    {
        term *= value / index;
        result += term;
    }
    while (exponent > 0)
    {
        result *= 2.0;
        exponent--;
    }
    while (exponent < 0)
    {
        result *= 0.5;
        exponent++;
    }
    return result;
}

double cps1_pow(double base, double exponent)
{
    if (base <= 0.0)
        return 0.0;
    return cps1_exp(exponent * cps1_log(base));
}

bool cps1_platform_allocation_failed(void)
{
    return allocation_failed;
}

void cps1_platform_reset_failure(void)
{
    allocation_failed = false;
    last_allocation_size = 0;
}

size_t cps1_platform_last_allocation_size(void)
{
    return last_allocation_size;
}

void logoutput(const char *format, ...)
{
    va_list ap;
    char line[192];

    va_start(ap, format);
    rb->vsnprintf(line, sizeof(line), format, ap);
    va_end(ap);
    DEBUGF("cps1: %s", line);
}
