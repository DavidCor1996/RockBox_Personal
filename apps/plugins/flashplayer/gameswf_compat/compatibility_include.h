/* Rockbox gameswf compatibility layer. */

#ifndef FLASHPLAYER_GAMESWF_COMPATIBILITY_INCLUDE_H
#define FLASHPLAYER_GAMESWF_COMPATIBILITY_INCLUDE_H

#include "lib/plugin_cxx_compat.h"
#include "lib/plugin_cxx.h"

typedef struct rb_compat_FILE FILE;

extern "C" {
void flashplayer_trace_tag(int tag_count, int tag_type, int stream_pos);
void flashplayer_trace_shape(int phase, int character_id, int stream_pos);
void flashplayer_trace_shape_record(int record_count, int flags, int stream_pos);
void flashplayer_trace_rect(int phase, int nbits, int stream_pos);
void flashplayer_trace_execute_tag(int frame);
void flashplayer_trace_loader(int hit, int tag_type);
void flashplayer_trace_loader_register(int tag_type);
void flashplayer_trace_font(int op, int font_id);
void *flashplayer_tu_realloc(void *ptr, size_t new_size, size_t old_size);
extern FILE *stderr;
int fprintf(FILE *stream, const char *format, ...);
int printf(const char *format, ...);
FILE *fopen(const char *path, const char *mode);
int fclose(FILE *stream);
size_t fread(void *ptr, size_t size, size_t nmemb, FILE *stream);
size_t fwrite(const void *ptr, size_t size, size_t nmemb, FILE *stream);
int fseek(FILE *stream, long offset, int whence);
long ftell(FILE *stream);
int feof(FILE *stream);
void clearerr(FILE *stream);
int fputc(int c, FILE *stream);
double strtod(const char *nptr, char **endptr);
double atof(const char *nptr);
}

#undef swap16
#undef swap32
#undef swap64
#undef str
#undef YEAR

#ifndef SIMULATOR
#ifndef isnan
#define isnan(x) __builtin_isnan(x)
#endif
#ifndef sinf
#define sinf(x) __builtin_sinf(x)
#endif
#ifndef cosf
#define cosf(x) __builtin_cosf(x)
#endif
#ifndef atan2f
#define atan2f(y, x) __builtin_atan2f((y), (x))
#endif
#ifndef sqrtf
#define sqrtf(x) __builtin_sqrtf(x)
#endif
#ifndef sqrt
#define sqrt(x) __builtin_sqrt(x)
#endif
#ifndef floor
#define floor(x) __builtin_floor(x)
#endif
#ifndef fabs
#define fabs(x) __builtin_fabs(x)
#endif
#ifndef acos
#define acos(x) __builtin_acos(x)
#endif
#ifndef asin
#define asin(x) __builtin_asin(x)
#endif
#ifndef atan
#define atan(x) __builtin_atan(x)
#endif
#ifndef atan2
#define atan2(y, x) __builtin_atan2((y), (x))
#endif
#ifndef cos
#define cos(x) __builtin_cos(x)
#endif
#ifndef exp
#define exp(x) __builtin_exp(x)
#endif
#ifndef log
#define log(x) __builtin_log(x)
#endif
#ifndef sin
#define sin(x) __builtin_sin(x)
#endif
#ifndef tan
#define tan(x) __builtin_tan(x)
#endif
#ifndef pow
#define pow(x, y) __builtin_pow((x), (y))
#endif
#ifndef fmod
#define fmod(x, y) __builtin_fmod((x), (y))
#endif
#ifndef fmodf
#define fmodf(x, y) __builtin_fmodf((x), (y))
#endif
#ifndef ceil
#define ceil(x) __builtin_ceil(x)
#endif
#ifndef log2
#define log2(x) __builtin_log2(x)
#endif
#ifndef floorf
#define floorf(x) __builtin_floorf(x)
#endif
#ifndef ceilf
#define ceilf(x) __builtin_ceilf(x)
#endif
#ifndef fabsf
#define fabsf(x) __builtin_fabsf(x)
#endif
#endif

#define TU_CONFIG_LINK_TO_THREAD 0
#define TU_CONFIG_LINK_TO_JPEGLIB 0
#define TU_CONFIG_LINK_TO_ZLIB 0
#define TU_CONFIG_LINK_TO_LIBPNG 0
#define TU_CONFIG_LINK_TO_FFMPEG 0
#define TU_CONFIG_LINK_TO_FREETYPE 0

#define TU_USE_OGLES_DISABLED_FOR_ROCKBOX 1
#define TU_USE_OPENAL_DISABLED_FOR_ROCKBOX 1

#define tu_malloc(size) operator new(size)
#define tu_free(ptr, old_size) operator delete(ptr)
#define tu_realloc(ptr, new_size, old_size) flashplayer_tu_realloc(ptr, new_size, old_size)

#define tu_error_exit(error_code, error_message) do { (void)(error_code); } while (0)

#endif
