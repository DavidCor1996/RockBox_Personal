#include "picodrive.h"
#include <stdarg.h>

struct pd_alloc_header
{
    size_t size;
};

void pd_arena_init(void *base, size_t size)
{
    uintptr_t aligned = ((uintptr_t)base + 15) & ~(uintptr_t)15;

    pd.arena_base = (unsigned char *)aligned;
    pd.arena_ptr = pd.arena_base;
    pd.arena_end = (unsigned char *)base + size;
    pd.arena_size = pd.arena_end > pd.arena_base ?
                    (size_t)(pd.arena_end - pd.arena_base) : 0;
    pd.arena_used = 0;
}

void *pd_malloc(size_t size)
{
    struct pd_alloc_header *header;
    size_t total;

    if (size == 0)
        size = 1;
    if (size > (size_t)-1 - sizeof(*header) - 15)
        return NULL;
    total = (size + sizeof(*header) + 15) & ~(size_t)15;
    if (!pd.arena_ptr || total > (size_t)(pd.arena_end - pd.arena_ptr))
    {
        pd.failed = true;
        return NULL;
    }
    header = (struct pd_alloc_header *)pd.arena_ptr;
    header->size = size;
    pd.arena_ptr += total;
    pd.arena_used += total;
    return header + 1;
}

void *pd_calloc(size_t count, size_t size)
{
    size_t total;
    void *result;

    if (count && size > (size_t)-1 / count)
        return NULL;
    total = count * size;
    result = pd_malloc(total);
    if (result)
        rb->memset(result, 0, total);
    return result;
}

void *pd_realloc(void *ptr, size_t size)
{
    struct pd_alloc_header *header;
    void *result;
    size_t copy;

    if (!ptr)
        return pd_malloc(size);
    if (size == 0)
        return NULL;
    header = (struct pd_alloc_header *)ptr - 1;
    result = pd_malloc(size);
    if (!result)
        return NULL;
    copy = header->size < size ? header->size : size;
    rb->memcpy(result, ptr, copy);
    return result;
}

void pd_free(void *ptr)
{
    (void)ptr;
}

void pd_abort(void)
{
    pd.failed = true;
}

int pd_printf(const char *format, ...)
{
    va_list ap;
    char line[192];
    int result;

    va_start(ap, format);
    result = rb->vsnprintf(line, sizeof(line), format, ap);
    va_end(ap);
    pd_log("core: %s", line);
    return result;
}

static unsigned long parse_unsigned(const char *text, char **end, int base)
{
    unsigned long value = 0;
    const char *cursor = text;

    while (*cursor == ' ' || *cursor == '\t')
        cursor++;
    if (base == 0)
    {
        if (cursor[0] == '0' && (cursor[1] == 'x' || cursor[1] == 'X'))
        {
            base = 16;
            cursor += 2;
        }
        else
            base = 10;
    }
    while (*cursor)
    {
        int digit;

        if (*cursor >= '0' && *cursor <= '9')
            digit = *cursor - '0';
        else if (*cursor >= 'a' && *cursor <= 'f')
            digit = *cursor - 'a' + 10;
        else if (*cursor >= 'A' && *cursor <= 'F')
            digit = *cursor - 'A' + 10;
        else
            break;
        if (digit >= base)
            break;
        value = value * (unsigned)base + (unsigned)digit;
        cursor++;
    }
    if (end)
        *end = (char *)cursor;
    return value;
}

long pd_strtol(const char *text, char **end, int base)
{
    bool negative = false;
    const char *cursor = text;
    unsigned long value;

    while (*cursor == ' ' || *cursor == '\t')
        cursor++;
    if (*cursor == '-')
    {
        negative = true;
        cursor++;
    }
    else if (*cursor == '+')
        cursor++;
    value = parse_unsigned(cursor, end, base);
    return negative ? -(long)value : (long)value;
}

unsigned long pd_strtoul(const char *text, char **end, int base)
{
    return parse_unsigned(text, end, base);
}

int pd_atoi(const char *text)
{
    return (int)pd_strtol(text, NULL, 10);
}

char *pd_strdup(const char *text)
{
    size_t size = rb->strlen(text) + 1;
    char *result = pd_malloc(size);

    if (result)
        rb->memcpy(result, text, size);
    return result;
}

char *pd_strncpy(char *destination, const char *source, size_t count)
{
    char *result = destination;

    while (count && *source)
    {
        *destination++ = *source++;
        count--;
    }
    while (count)
    {
        *destination++ = '\0';
        count--;
    }
    return result;
}

int pd_abs(int value)
{
    return value < 0 ? -value : value;
}

double pd_fabs(double value)
{
    return value < 0.0 ? -value : value;
}

double pd_floor(double value)
{
    long integer = (long)value;

    if ((double)integer > value)
        integer--;
    return (double)integer;
}

double pd_round(double value)
{
    return value < 0.0 ? -pd_floor(-value + 0.5) : pd_floor(value + 0.5);
}

double pd_sin(double value)
{
    const double pi = 3.14159265358979323846;
    const double two_pi = 6.28318530717958647692;
    double term;
    double sum;
    double square;
    int index;

    while (value > pi)
        value -= two_pi;
    while (value < -pi)
        value += two_pi;
    if (value > pi / 2.0)
        value = pi - value;
    else if (value < -pi / 2.0)
        value = -pi - value;

    square = value * value;
    term = value;
    sum = value;
    for (index = 1; index < 10; index++)
    {
        term *= -square / ((2.0 * index) * (2.0 * index + 1.0));
        sum += term;
    }
    return sum;
}

double pd_cos(double value)
{
    return pd_sin(value + 1.57079632679489661923);
}

double pd_sqrt(double value)
{
    double guess;
    int index;

    if (value <= 0.0)
        return 0.0;
    guess = value > 1.0 ? value : 1.0;
    for (index = 0; index < 16; index++)
        guess = 0.5 * (guess + value / guess);
    return guess;
}

double pd_log_math(double value)
{
    const double ln2 = 0.69314718055994530942;
    double y;
    double term;
    double sum;
    int power;
    int scale = 0;

    if (value <= 0.0)
        return -1.0e30;
    while (value > 1.4142135623730951)
    {
        value *= 0.5;
        scale++;
    }
    while (value < 0.7071067811865476)
    {
        value *= 2.0;
        scale--;
    }
    y = (value - 1.0) / (value + 1.0);
    term = y;
    sum = y;
    for (power = 3; power <= 25; power += 2)
    {
        term *= y * y;
        sum += term / power;
    }
    return 2.0 * sum + scale * ln2;
}

static double pd_exp(double value)
{
    const double ln2 = 0.69314718055994530942;
    double term = 1.0;
    double sum = 1.0;
    int scale = 0;
    int index;

    while (value > ln2 / 2.0)
    {
        value -= ln2;
        scale++;
    }
    while (value < -ln2 / 2.0)
    {
        value += ln2;
        scale--;
    }
    for (index = 1; index < 18; index++)
    {
        term *= value / index;
        sum += term;
    }
    while (scale > 0)
    {
        sum *= 2.0;
        scale--;
    }
    while (scale < 0)
    {
        sum *= 0.5;
        scale++;
    }
    return sum;
}

double pd_pow(double base, double exponent)
{
    return pd_exp(exponent * pd_log_math(base));
}

void pd_log(const char *format, ...)
{
    va_list ap;
    char line[256];
    int fd;

    va_start(ap, format);
    rb->vsnprintf(line, sizeof(line), format, ap);
    va_end(ap);
    fd = rb->open(PD_LOG_PATH, O_WRONLY | O_CREAT | O_APPEND, 0666);
    if (fd >= 0)
    {
        rb->fdprintf(fd, "%s\n", line);
        rb->close(fd);
    }
}

void *plat_mmap(unsigned long addr, size_t size, int need_exec, int is_fixed)
{
    (void)addr;
    (void)need_exec;
    (void)is_fixed;
    return pd_malloc(size);
}

void *plat_mremap(void *ptr, size_t oldsize, size_t newsize)
{
    void *result = pd_malloc(newsize);

    if (result && ptr)
        rb->memcpy(result, ptr, oldsize < newsize ? oldsize : newsize);
    return result;
}

void plat_munmap(void *ptr, size_t size)
{
    (void)ptr;
    (void)size;
}

void *plat_mem_get_for_drc(size_t size)
{
    (void)size;
    return NULL;
}

int plat_mem_set_exec(void *ptr, size_t size)
{
    (void)ptr;
    (void)size;
    return -1;
}

void cache_flush_d_inval_i(void *start, void *end)
{
#ifdef CPU_ARM
    rb->commit_discard_idcache();
    (void)start;
    (void)end;
#else
    (void)start;
    (void)end;
#endif
}

void emu_video_mode_change(int start_line, int line_count,
                           int start_col, int col_count)
{
    (void)start_line;
    (void)line_count;
    (void)start_col;
    (void)col_count;
}

void emu_32x_startup(void)
{
}

void *p32x_bios_g;
void *p32x_bios_m;
void *p32x_bios_s;
