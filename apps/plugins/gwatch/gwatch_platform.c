#include "gwatch_platform.h"
#include <tlsf.h>
#include <stdint.h>

#undef malloc
#undef calloc
#undef realloc
#undef free
#undef abort
#undef exit
#undef getenv
#undef system
#undef fopen
#undef freopen
#undef tmpfile
#undef fclose
#undef fflush
#undef ferror
#undef feof
#undef fgetc
#undef getc
#undef ungetc
#undef clearerr
#undef fread
#undef fwrite
#undef fprintf
#undef fgets
#undef fputs
#undef localeconv
#undef strtod
#undef strpbrk
#undef strspn
#undef strcoll
#undef strerror
#undef strncmp
#undef memchr
#undef srand
#undef rand
#undef vsnprintf
#undef time
#undef localtime
#undef floor
#undef ceil
#undef fmod
#undef pow
#undef tan
#undef sqrt
#undef sin
#undef cos
#undef log10
#undef log
#undef exp
#undef atan2
#undef asin
#undef acos
#undef fabs
#undef frexp

#define GWATCH_PI 3.14159265358979323846
#define GWATCH_HALF_PI 1.57079632679489661923
#define GWATCH_LN10 2.30258509299404568402
#define GWATCH_MIN_POOL_SIZE (2 * 1024 * 1024)
#define GWATCH_MAX_POOL_SIZE (8 * 1024 * 1024)

static void *pool;
static size_t pool_size;
static bool pool_from_audio_buffer;
static struct tm fallback_tm;
static struct lconv c_locale = { "." };

bool gwatch_platform_init_memory(void)
{
    pool = rb->plugin_get_audio_buffer(&pool_size);
    pool_from_audio_buffer = pool != NULL;
    if (pool == NULL || pool_size < GWATCH_MIN_POOL_SIZE)
    {
        gwatch_platform_release_memory();
        return false;
    }

#if (CONFIG_PLATFORM & PLATFORM_NATIVE)
    if ((uintptr_t)pool < (uintptr_t)plugin_start_addr)
    {
        size_t overlay_limit = (uintptr_t)plugin_start_addr - (uintptr_t)pool;
        if (pool_size > overlay_limit)
            pool_size = overlay_limit;
    }
#endif

    if (pool_size > GWATCH_MAX_POOL_SIZE)
        pool_size = GWATCH_MAX_POOL_SIZE;
    if (pool_size < GWATCH_MIN_POOL_SIZE ||
        init_memory_pool(pool_size, pool) == (size_t)-1)
    {
        gwatch_platform_release_memory();
        return false;
    }

    return true;
}

void gwatch_platform_release_memory(void)
{
    if (pool != NULL)
    {
        destroy_memory_pool(pool);
        pool = NULL;
        pool_size = 0;
    }

    if (pool_from_audio_buffer)
    {
        rb->plugin_release_audio_buffer();
        pool_from_audio_buffer = false;
    }
}

void *gwatch_malloc(size_t size)
{
    if (size == 0)
        return NULL;
    return tlsf_malloc(size);
}

void *gwatch_calloc(size_t nmemb, size_t size)
{
    size_t bytes = nmemb * size;
    void *ptr;

    if (nmemb != 0 && bytes / nmemb != size)
        return NULL;

    ptr = gwatch_malloc(bytes);
    if (ptr != NULL)
        rb->memset(ptr, 0, bytes);
    return ptr;
}

void *gwatch_realloc(void *ptr, size_t size)
{
    if (size == 0)
    {
        gwatch_free(ptr);
        return NULL;
    }
    return tlsf_realloc(ptr, size);
}

void gwatch_free(void *ptr)
{
    if (ptr != NULL)
        tlsf_free(ptr);
}

void *gwrom_malloc(size_t size)
{
    return gwatch_malloc(size);
}

void *gwrom_realloc(void *ptr, size_t size)
{
    return gwatch_realloc(ptr, size);
}

void gwrom_free(void *ptr)
{
    gwatch_free(ptr);
}

void *gwlua_malloc(size_t size)
{
    return gwatch_malloc(size);
}

void *gwlua_realloc(void *ptr, size_t size)
{
    return gwatch_realloc(ptr, size);
}

void gwlua_free(void *ptr)
{
    gwatch_free(ptr);
}

time_t gwatch_time(time_t *timer)
{
    time_t now = *rb->current_tick / HZ;

#if CONFIG_RTC
    struct tm *tm = rb->get_time();
    if (tm != NULL)
        now = rb->mktime(tm);
#endif

    if (timer != NULL)
        *timer = now;
    return now;
}

struct tm *gwatch_localtime(const time_t *timer)
{
    (void)timer;

    if (rb->get_time() != NULL)
        return rb->get_time();

    rb->memset(&fallback_tm, 0, sizeof(fallback_tm));
    fallback_tm.tm_mday = 1;
    return &fallback_tm;
}

char *gwatch_getenv(const char *name)
{
    (void)name;
    return NULL;
}

int gwatch_system(const char *command)
{
    (void)command;
    return -1;
}

FILE *gwatch_fopen(const char *path, const char *mode)
{
    (void)path;
    (void)mode;
    return NULL;
}

FILE *gwatch_freopen(const char *path, const char *mode, FILE *stream)
{
    (void)path;
    (void)mode;
    (void)stream;
    return NULL;
}

FILE *gwatch_tmpfile(void)
{
    return NULL;
}

int gwatch_fclose(FILE *stream)
{
    (void)stream;
    return 0;
}

int gwatch_fflush(FILE *stream)
{
    (void)stream;
    return 0;
}

int gwatch_ferror(FILE *stream)
{
    (void)stream;
    return 0;
}

int gwatch_feof(FILE *stream)
{
    (void)stream;
    return 1;
}

int gwatch_fgetc(FILE *stream)
{
    (void)stream;
    return EOF;
}

int gwatch_getc(FILE *stream)
{
    return gwatch_fgetc(stream);
}

int gwatch_ungetc(int c, FILE *stream)
{
    (void)c;
    (void)stream;
    return EOF;
}

void gwatch_clearerr(FILE *stream)
{
    (void)stream;
}

size_t gwatch_fread(void *ptr, size_t size, size_t nmemb, FILE *stream)
{
    (void)ptr;
    (void)size;
    (void)nmemb;
    (void)stream;
    return 0;
}

size_t gwatch_fwrite(const void *ptr, size_t size, size_t nmemb, FILE *stream)
{
    (void)ptr;
    (void)size;
    (void)nmemb;
    (void)stream;
    return 0;
}

int gwatch_fprintf(FILE *stream, const char *format, ...)
{
    (void)stream;
    (void)format;
    return 0;
}

char *gwatch_fgets(char *str, int count, FILE *stream)
{
    (void)str;
    (void)count;
    (void)stream;
    return NULL;
}

int gwatch_fputs(const char *str, FILE *stream)
{
    (void)str;
    (void)stream;
    return EOF;
}

struct lconv *gwatch_localeconv(void)
{
    return &c_locale;
}

double gwatch_strtod(const char *nptr, char **endptr)
{
    const char *start = nptr;
    const char *p = nptr;
    double value = 0.0;
    double scale = 1.0;
    int sign = 1;
    int exp_sign = 1;
    int exponent = 0;
    bool have_digit = false;

    while (*p == ' ' || *p == '\t' || *p == '\n' ||
           *p == '\r' || *p == '\f' || *p == '\v')
        p++;

    if (*p == '+' || *p == '-')
    {
        if (*p == '-')
            sign = -1;
        p++;
    }

    while (*p >= '0' && *p <= '9')
    {
        value = value * 10.0 + (double)(*p - '0');
        p++;
        have_digit = true;
    }

    if (*p == '.')
    {
        p++;
        while (*p >= '0' && *p <= '9')
        {
            value = value * 10.0 + (double)(*p - '0');
            scale *= 10.0;
            p++;
            have_digit = true;
        }
    }

    if (!have_digit)
    {
        if (endptr != NULL)
            *endptr = (char *)start;
        return 0.0;
    }

    if (*p == 'e' || *p == 'E')
    {
        const char *exp_start = p;
        bool have_exp_digit = false;

        p++;
        if (*p == '+' || *p == '-')
        {
            if (*p == '-')
                exp_sign = -1;
            p++;
        }

        while (*p >= '0' && *p <= '9')
        {
            if (exponent < 308)
                exponent = exponent * 10 + (*p - '0');
            p++;
            have_exp_digit = true;
        }

        if (!have_exp_digit)
            p = exp_start;
    }

    value = (double)sign * value / scale;
    while (exponent-- > 0)
        value = exp_sign > 0 ? value * 10.0 : value / 10.0;

    if (endptr != NULL)
        *endptr = (char *)p;
    return value;
}

char *gwatch_strpbrk(const char *s, const char *accept)
{
    const char *p;

    for (; *s != '\0'; s++)
    {
        for (p = accept; *p != '\0'; p++)
        {
            if (*s == *p)
                return (char *)s;
        }
    }
    return NULL;
}

size_t gwatch_strspn(const char *s, const char *accept)
{
    size_t count = 0;
    const char *p;
    bool matched;

    for (; *s != '\0'; s++, count++)
    {
        matched = false;
        for (p = accept; *p != '\0'; p++)
        {
            if (*s == *p)
            {
                matched = true;
                break;
            }
        }
        if (!matched)
            break;
    }
    return count;
}

int gwatch_strcoll(const char *s1, const char *s2)
{
    return rb->strcmp(s1, s2);
}

char *gwatch_strerror(int errnum)
{
    (void)errnum;
    return "error";
}

void gwatch_abort(void)
{
    rb->splash(HZ, "Game & Watch core abort");
}

void gwatch_exit(int status)
{
    (void)status;
}

double gwatch_floor(double x)
{
    long i = (long)x;

    if ((double)i > x)
        i--;
    return (double)i;
}

double gwatch_ceil(double x)
{
    long i = (long)x;

    if ((double)i < x)
        i++;
    return (double)i;
}

double gwatch_fabs(double x)
{
    return x < 0.0 ? -x : x;
}

double gwatch_fmod(double x, double y)
{
    long q;

    if (y == 0.0)
        return 0.0;
    q = (long)(x / y);
    return x - (double)q * y;
}

double gwatch_sqrt(double x)
{
    double guess;
    int i;

    if (x <= 0.0)
        return 0.0;

    guess = x > 1.0 ? x : 1.0;
    for (i = 0; i < 16; i++)
        guess = 0.5 * (guess + x / guess);
    return guess;
}

static double reduce_angle(double x)
{
    while (x > GWATCH_PI)
        x -= 2.0 * GWATCH_PI;
    while (x < -GWATCH_PI)
        x += 2.0 * GWATCH_PI;
    return x;
}

double gwatch_sin(double x)
{
    double x2;

    x = reduce_angle(x);
    x2 = x * x;
    return x * (1.0 - x2 / 6.0 + x2 * x2 / 120.0 -
                x2 * x2 * x2 / 5040.0);
}

double gwatch_cos(double x)
{
    double x2;

    x = reduce_angle(x);
    x2 = x * x;
    return 1.0 - x2 / 2.0 + x2 * x2 / 24.0 -
           x2 * x2 * x2 / 720.0;
}

double gwatch_tan(double x)
{
    double c = gwatch_cos(x);

    if (c == 0.0)
        return 0.0;
    return gwatch_sin(x) / c;
}

double gwatch_exp(double x)
{
    double term = 1.0;
    double sum = 1.0;
    int i;

    if (x < -20.0)
        return 0.0;
    if (x > 20.0)
        x = 20.0;

    for (i = 1; i < 32; i++)
    {
        term *= x / (double)i;
        sum += term;
    }
    return sum;
}

double gwatch_log(double x)
{
    double y;
    double y2;
    double term;
    double sum;
    int i;

    if (x <= 0.0)
        return 0.0;

    while (x > 2.0)
        x *= 0.5;
    while (x < 0.5)
        x *= 2.0;

    y = (x - 1.0) / (x + 1.0);
    y2 = y * y;
    term = y;
    sum = 0.0;
    for (i = 1; i < 32; i += 2)
    {
        sum += term / (double)i;
        term *= y2;
    }
    return 2.0 * sum;
}

double gwatch_log10(double x)
{
    return gwatch_log(x) / GWATCH_LN10;
}

double gwatch_pow(double x, double y)
{
    long iy = (long)y;
    double result = 1.0;
    long i;

    if (x == 0.0)
        return 0.0;

    if ((double)iy == y && iy >= 0 && iy < 64)
    {
        for (i = 0; i < iy; i++)
            result *= x;
        return result;
    }

    if (x < 0.0)
        return 0.0;
    return gwatch_exp(gwatch_log(x) * y);
}

double gwatch_atan2(double y, double x)
{
    double abs_y = gwatch_fabs(y) + 1e-10;
    double r;
    double angle;

    if (x < 0.0)
    {
        r = (x + abs_y) / (abs_y - x);
        angle = 3.0 * GWATCH_PI / 4.0;
    }
    else
    {
        r = (x - abs_y) / (x + abs_y);
        angle = GWATCH_PI / 4.0;
    }

    angle += (0.1963 * r * r - 0.9817) * r;
    return y < 0.0 ? -angle : angle;
}

double gwatch_asin(double x)
{
    if (x > 1.0)
        x = 1.0;
    if (x < -1.0)
        x = -1.0;
    return gwatch_atan2(x, gwatch_sqrt(1.0 - x * x));
}

double gwatch_acos(double x)
{
    return GWATCH_HALF_PI - gwatch_asin(x);
}

double gwatch_frexp(double x, int *exp)
{
    int e = 0;
    double value = x;
    double sign = 1.0;

    if (value == 0.0)
    {
        if (exp != NULL)
            *exp = 0;
        return 0.0;
    }

    if (value < 0.0)
    {
        sign = -1.0;
        value = -value;
    }

    while (value >= 1.0)
    {
        value *= 0.5;
        e++;
    }
    while (value < 0.5)
    {
        value *= 2.0;
        e--;
    }

    if (exp != NULL)
        *exp = e;
    return sign * value;
}
