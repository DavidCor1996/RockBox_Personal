#include "compatibility_include.h"
#include "base/jpeg.h"
#include "base/tu_loadlib.h"
#include "base/tu_timer.h"
#include "gameswf/gameswf_abc.h"
#include "gameswf/gameswf_video_impl.h"

#undef floor
#undef floorf
#undef ceil
#undef ceilf
#undef fmod
#undef fmodf
#undef sqrt
#undef sqrtf
#undef sin
#undef sinf
#undef cos
#undef cosf
#undef tan
#undef atan
#undef atan2
#undef atan2f
#undef asin
#undef acos
#undef exp
#undef log
#undef pow

#ifdef SIMULATOR
extern "C" float fabsf(float x) { return x < 0.0f ? -x : x; }
extern "C" float fmodf(float x, float y)
{
    int q;
    if (y == 0.0f)
        return 0.0f;
    q = (int)(x / y);
    return x - (float)q * y;
}
extern "C" double fabs(double x) { return x < 0.0 ? -x : x; }
#endif

struct rb_compat_file {
    int fd;
    bool eof;
};

FILE *stderr = 0;

static int mode_to_flags(const char *mode)
{
    if (!mode || !mode[0])
        return O_RDONLY;

    if (mode[0] == 'w')
        return O_WRONLY | O_CREAT | O_TRUNC;
    if (mode[0] == 'a')
        return O_WRONLY | O_CREAT | O_APPEND;

    return O_RDONLY;
}

extern "C" FILE *fopen(const char *path, const char *mode)
{
    int fd = rb->open(path, mode_to_flags(mode), 0666);
    rb_compat_file *file;

    if (fd < 0)
        return 0;

    file = new rb_compat_file;
    if (!file) {
        rb->close(fd);
        return 0;
    }

    file->fd = fd;
    file->eof = false;
    return (FILE *)file;
}

extern "C" int fclose(FILE *stream)
{
    rb_compat_file *file = (rb_compat_file *)stream;
    int rc;

    if (!file)
        return EOF;

    rc = rb->close(file->fd);
    delete file;
    return rc < 0 ? EOF : 0;
}

extern "C" size_t fread(void *ptr, size_t size, size_t nmemb, FILE *stream)
{
    rb_compat_file *file = (rb_compat_file *)stream;
    size_t bytes = size * nmemb;
    ssize_t got;

    if (!file || !ptr || size == 0 || nmemb == 0)
        return 0;

    got = rb->read(file->fd, ptr, bytes);
    if (got <= 0) {
        file->eof = true;
        return 0;
    }

    if ((size_t)got < bytes)
        file->eof = true;

    return (size_t)got / size;
}

extern "C" size_t fwrite(const void *ptr, size_t size, size_t nmemb, FILE *stream)
{
    rb_compat_file *file = (rb_compat_file *)stream;
    size_t bytes = size * nmemb;
    ssize_t wrote;

    if (!file || !ptr || size == 0 || nmemb == 0)
        return 0;

    wrote = rb->write(file->fd, ptr, bytes);
    if (wrote <= 0)
        return 0;

    return (size_t)wrote / size;
}

extern "C" int fseek(FILE *stream, long offset, int whence)
{
    rb_compat_file *file = (rb_compat_file *)stream;
    int rc;

    if (!file)
        return EOF;

    rc = rb->lseek(file->fd, offset, whence);
    if (rc < 0)
        return EOF;

    file->eof = false;
    return 0;
}

extern "C" long ftell(FILE *stream)
{
    rb_compat_file *file = (rb_compat_file *)stream;

    if (!file)
        return -1;

    return rb->lseek(file->fd, 0, SEEK_CUR);
}

extern "C" int feof(FILE *stream)
{
    rb_compat_file *file = (rb_compat_file *)stream;
    return file ? file->eof : 1;
}

extern "C" void clearerr(FILE *stream)
{
    rb_compat_file *file = (rb_compat_file *)stream;
    if (file)
        file->eof = false;
}

extern "C" int fputc(int c, FILE *stream)
{
    unsigned char ch = (unsigned char)c;
    return fwrite(&ch, 1, 1, stream) == 1 ? ch : EOF;
}

extern "C" int fprintf(FILE *stream, const char *format, ...)
{
    (void)stream;
    (void)format;
    return 0;
}

extern "C" int printf(const char *format, ...)
{
    (void)format;
    return 0;
}

void __assert(const char *file, int line, const char *expr)
{
    (void)file;
    (void)line;
    (void)expr;
}

#ifdef SIMULATOR
extern "C" size_t strlen(const char *s)
{
    if (!rb) {
        const char *p = s;
        while (*p)
            p++;
        return (size_t)(p - s);
    }
    return rb->strlen(s);
}

extern "C" char *strcpy(char *dest, const char *src)
{
    char *out = dest;
    while ((*dest++ = *src++))
        ;
    return out;
}

extern "C" int strcmp(const char *a, const char *b)
{
    if (!rb) {
        while (*a && *a == *b) {
            a++;
            b++;
        }
        return (unsigned char)*a - (unsigned char)*b;
    }
    return rb->strcmp(a, b);
}
#endif

extern "C" int strncmp(const char *a, const char *b, size_t n)
{
    while (n-- > 0) {
        unsigned char ca = (unsigned char)*a++;
        unsigned char cb = (unsigned char)*b++;
        if (ca != cb || ca == '\0' || cb == '\0')
            return ca - cb;
    }
    return 0;
}

#ifdef SIMULATOR
extern "C" int toupper(int c)
{
    return c >= 'a' && c <= 'z' ? c - ('a' - 'A') : c;
}

extern "C" int tolower(int c)
{
    return c >= 'A' && c <= 'Z' ? c + ('a' - 'A') : c;
}
#endif

extern "C" int strcasecmp(const char *a, const char *b)
{
    while (*a || *b) {
        int ca = tolower((unsigned char)*a++);
        int cb = tolower((unsigned char)*b++);
        if (ca != cb)
            return ca - cb;
    }
    return 0;
}

extern "C" int strncasecmp(const char *a, const char *b, size_t n)
{
    while (n-- > 0) {
        int ca = tolower((unsigned char)*a++);
        int cb = tolower((unsigned char)*b++);
        if (ca != cb || ca == 0 || cb == 0)
            return ca - cb;
    }
    return 0;
}

extern "C" int vsnprintf(char *str, size_t size, const char *format, va_list ap)
{
    return rb->vsnprintf(str, size, format, ap);
}

extern "C" int snprintf(char *str, size_t size, const char *format, ...)
{
    int rc;
    va_list ap;

    va_start(ap, format);
    rc = rb->vsnprintf(str, size, format, ap);
    va_end(ap);
    return rc;
}

extern "C" double log2(double x)
{
    int bits = 0;

    while (x > 1.0) {
        x *= 0.5;
        bits++;
    }

    return (double)bits;
}

extern "C" double floor(double x)
{
    int i = (int)x;
    return (x < 0.0 && (double)i != x) ? (double)(i - 1) : (double)i;
}

extern "C" float floorf(float x)
{
    int i = (int)x;
    return (x < 0.0f && (float)i != x) ? (float)(i - 1) : (float)i;
}

extern "C" double ceil(double x)
{
    int i = (int)x;
    return (x > 0.0 && (double)i != x) ? (double)(i + 1) : (double)i;
}

extern "C" float ceilf(float x)
{
    int i = (int)x;
    return (x > 0.0f && (float)i != x) ? (float)(i + 1) : (float)i;
}

extern "C" double fmod(double x, double y)
{
    int q;
    if (y == 0.0)
        return 0.0;
    q = (int)(x / y);
    return x - (double)q * y;
}

extern "C" float sqrtf(float x);

extern "C" double sqrt(double x)
{
    return (double)sqrtf((float)x);
}

extern "C" float sqrtf(float x)
{
    float guess;
    int i;

    if (x <= 0.0f)
        return 0.0f;

    guess = x > 1.0f ? x : 1.0f;
    for (i = 0; i < 8; i++)
        guess = 0.5f * (guess + x / guess);

    return guess;
}

extern "C" float sinf(float x)
{
    float x2 = x * x;
    return x * (1.0f - x2 / 6.0f + (x2 * x2) / 120.0f);
}

extern "C" float cosf(float x)
{
    float x2 = x * x;
    return 1.0f - x2 / 2.0f + (x2 * x2) / 24.0f;
}

extern "C" float atan2f(float y, float x)
{
    if (x == 0.0f)
        return y >= 0.0f ? 1.5707963f : -1.5707963f;
    return y / x;
}

extern "C" double sin(double x) { return (double)sinf((float)x); }
extern "C" double cos(double x) { return (double)cosf((float)x); }
extern "C" double tan(double x) { double c = cos(x); return c == 0.0 ? 0.0 : sin(x) / c; }
extern "C" double atan(double x) { return x; }
extern "C" double atan2(double y, double x) { return (double)atan2f((float)y, (float)x); }
extern "C" double asin(double x) { return x; }
extern "C" double acos(double x) { return 1.57079632679 - x; }
extern "C" double exp(double x) { return 1.0 + x + x * x * 0.5; }
extern "C" double log(double x) { return x > 0.0 ? x - 1.0 : 0.0; }
extern "C" double pow(double x, double y) { (void)y; return x; }

static int digit_value(char c)
{
    return c >= '0' && c <= '9' ? c - '0' : -1;
}

extern "C" double strtod(const char *nptr, char **endptr)
{
    const char *p = nptr;
    double value = 0.0;
    double scale = 0.1;
    int sign = 1;
    int digit;

    while (*p == ' ' || *p == '\t' || *p == '\n' || *p == '\r')
        p++;

    if (*p == '-') {
        sign = -1;
        p++;
    } else if (*p == '+') {
        p++;
    }

    while ((digit = digit_value(*p)) >= 0) {
        value = value * 10.0 + digit;
        p++;
    }

    if (*p == '.') {
        p++;
        while ((digit = digit_value(*p)) >= 0) {
            value += digit * scale;
            scale *= 0.1;
            p++;
        }
    }

    if (endptr)
        *endptr = (char *)p;

    return sign < 0 ? -value : value;
}

extern "C" double atof(const char *nptr)
{
    return strtod(nptr, 0);
}

#ifdef SIMULATOR
extern "C" long strtol(const char *nptr, char **endptr, int base)
{
    const char *p = nptr;
    long value = 0;
    int sign = 1;
    int digit;

    while (*p == ' ' || *p == '\t' || *p == '\n' || *p == '\r')
        p++;

    if (*p == '-') {
        sign = -1;
        p++;
    } else if (*p == '+') {
        p++;
    }

    if (base == 0)
        base = 10;

    while ((digit = digit_value(*p)) >= 0 && digit < base) {
        value = value * base + digit;
        p++;
    }

    if (endptr)
        *endptr = (char *)p;

    return value * sign;
}

extern "C" int atoi(const char *nptr)
{
    return (int)strtol(nptr, 0, 10);
}
#endif

extern "C" int atexit(void (*function)(void))
{
    (void)function;
    return 0;
}

#ifdef SIMULATOR
extern "C" void *malloc(size_t size)
{
    return operator new(size);
}

extern "C" void free(void *ptr)
{
    operator delete(ptr);
}

extern "C" void qsort(void *base, size_t nmemb, size_t size,
                      int (*compar)(const void *, const void *))
{
    unsigned char *bytes = (unsigned char *)base;
    size_t i;
    size_t j;
    size_t k;

    for (i = 0; i < nmemb; i++) {
        for (j = i + 1; j < nmemb; j++) {
            unsigned char *a = bytes + i * size;
            unsigned char *b = bytes + j * size;
            if (compar(a, b) > 0) {
                for (k = 0; k < size; k++) {
                    unsigned char t = a[k];
                    a[k] = b[k];
                    b[k] = t;
                }
            }
        }
    }
}
#endif

namespace tu_timer
{
    void init_timer()
    {
    }

    Uint32 get_ticks()
    {
        return *rb->current_tick * 1000 / HZ;
    }

    void sleep(int milliseconds)
    {
        rb->sleep(milliseconds * HZ / 1000);
    }

    uint64 get_profile_ticks()
    {
        return (uint64)*rb->current_tick;
    }

    double profile_ticks_to_seconds(uint64 ticks)
    {
        return (double)ticks / (double)HZ;
    }

    double profile_ticks_to_milliseconds(uint64 ticks)
    {
        return profile_ticks_to_seconds(ticks) * 1000.0;
    }

    Uint64 get_systime()
    {
        return (Uint64)*rb->current_tick / HZ;
    }
}

tu_datetime::tu_datetime() : m_time(0)
{
}

double tu_datetime::get_time() const
{
    return (double)m_time;
}

void tu_datetime::set_time(double t)
{
    m_time = (time_t)t;
}

int tu_datetime::get(part p)
{
    (void)p;
    return 0;
}

void tu_datetime::set(part p, int val)
{
    (void)p;
    (void)val;
}

tu_loadlib::tu_loadlib(const char *library_name) : m_hlib(0)
{
    (void)library_name;
}

tu_loadlib::~tu_loadlib()
{
}

void *tu_loadlib::get_function(const char *function_name)
{
    (void)function_name;
    return 0;
}

namespace jpeg
{
    input::~input()
    {
    }

    output::~output()
    {
    }

    input *input::create(tu_file *in)
    {
        (void)in;
        return 0;
    }

    input *input::create_swf_jpeg2_header_only(tu_file *in)
    {
        (void)in;
        return 0;
    }

    output *output::create(tu_file *out, int width, int height, int quality)
    {
        (void)out;
        (void)width;
        (void)height;
        (void)quality;
        return 0;
    }
}

namespace gameswf
{
    abc_def::abc_def(player *player)
    {
        (void)player;
    }

    abc_def::~abc_def()
    {
    }

    void abc_def::read(stream *in, movie_definition_sub *m)
    {
        (void)in;
        (void)m;
    }

    as_function *abc_def::get_script_function(const tu_string &name) const
    {
        (void)name;
        return 0;
    }

    as_function *abc_def::get_class_constructor(const tu_string &name) const
    {
        (void)name;
        return 0;
    }

    instance_info *abc_def::get_instance_info(const tu_string &class_name) const
    {
        (void)class_name;
        return 0;
    }

    video_stream_instance::video_stream_instance(player *player,
        video_stream_definition *def, character *parent, int id)
        : character(player, parent, id), m_def(def)
    {
    }

    video_stream_instance::~video_stream_instance()
    {
    }

    void video_stream_instance::display()
    {
    }

    character *video_stream_definition::create_character_instance(character *parent,
                                                                  int id)
    {
        return new video_stream_instance(get_player(), this, parent, id);
    }

    void video_stream_definition::read(stream *in, int tag, movie_definition *m)
    {
        (void)in;
        (void)tag;
        (void)m;
    }

    void video_stream_definition::get_bound(rect *bound)
    {
        bound->set_to_point(0.0f, 0.0f);
    }

    void clear_disasm()
    {
    }
}
