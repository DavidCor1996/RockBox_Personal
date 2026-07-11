#ifndef GWATCH_COMPAT_H
#define GWATCH_COMPAT_H

#include <stddef.h>
#include <locale.h>
#include <stdio.h>
#include <time.h>
#include "plugin.h"

#ifndef HUGE_VAL
#define HUGE_VAL 1.0e300
#endif

#ifndef BUFSIZ
#define BUFSIZ 512
#endif
#ifndef stdin
#define stdin ((FILE *)0)
#endif
#ifndef stdout
#define stdout ((FILE *)0)
#endif
#ifndef stderr
#define stderr ((FILE *)0)
#endif

#ifndef SIMULATOR
typedef void FILE;
#endif

void *gwatch_malloc(size_t size);
void *gwatch_calloc(size_t nmemb, size_t size);
void *gwatch_realloc(void *ptr, size_t size);
void gwatch_free(void *ptr);
time_t gwatch_time(time_t *timer);
struct tm *gwatch_localtime(const time_t *timer);
char *gwatch_getenv(const char *name);
int gwatch_system(const char *command);
FILE *gwatch_fopen(const char *path, const char *mode);
FILE *gwatch_freopen(const char *path, const char *mode, FILE *stream);
FILE *gwatch_tmpfile(void);
int gwatch_fclose(FILE *stream);
int gwatch_fflush(FILE *stream);
int gwatch_ferror(FILE *stream);
int gwatch_feof(FILE *stream);
int gwatch_fgetc(FILE *stream);
int gwatch_getc(FILE *stream);
int gwatch_ungetc(int c, FILE *stream);
void gwatch_clearerr(FILE *stream);
size_t gwatch_fread(void *ptr, size_t size, size_t nmemb, FILE *stream);
size_t gwatch_fwrite(const void *ptr, size_t size, size_t nmemb, FILE *stream);
int gwatch_fprintf(FILE *stream, const char *format, ...);
char *gwatch_fgets(char *str, int count, FILE *stream);
int gwatch_fputs(const char *str, FILE *stream);
struct lconv *gwatch_localeconv(void);
double gwatch_strtod(const char *nptr, char **endptr);
char *gwatch_strpbrk(const char *s, const char *accept);
size_t gwatch_strspn(const char *s, const char *accept);
int gwatch_strcoll(const char *s1, const char *s2);
char *gwatch_strerror(int errnum);
void gwatch_abort(void);
void gwatch_exit(int status);
double gwatch_floor(double x);
double gwatch_ceil(double x);
double gwatch_fabs(double x);
double gwatch_fmod(double x, double y);
double gwatch_sqrt(double x);
double gwatch_sin(double x);
double gwatch_cos(double x);
double gwatch_tan(double x);
double gwatch_exp(double x);
double gwatch_log(double x);
double gwatch_log10(double x);
double gwatch_pow(double x, double y);
double gwatch_atan2(double y, double x);
double gwatch_asin(double x);
double gwatch_acos(double x);
double gwatch_frexp(double x, int *exp);

#define malloc gwatch_malloc
#define calloc gwatch_calloc
#define realloc gwatch_realloc
#define free gwatch_free
#define abort gwatch_abort
#define exit gwatch_exit
#define getenv gwatch_getenv
#define system gwatch_system
#define fopen gwatch_fopen
#define freopen gwatch_freopen
#define tmpfile gwatch_tmpfile
#define fclose gwatch_fclose
#define fflush gwatch_fflush
#define ferror gwatch_ferror
#define feof gwatch_feof
#define fgetc gwatch_fgetc
#define getc gwatch_getc
#define ungetc gwatch_ungetc
#define clearerr gwatch_clearerr
#define fread gwatch_fread
#define fwrite gwatch_fwrite
#define fprintf gwatch_fprintf
#define fgets gwatch_fgets
#define fputs gwatch_fputs
#define localeconv gwatch_localeconv
#define strtod gwatch_strtod
#ifdef strpbrk
#undef strpbrk
#endif
#define strpbrk gwatch_strpbrk
#ifdef strspn
#undef strspn
#endif
#define strspn gwatch_strspn
#ifdef strcoll
#undef strcoll
#endif
#define strcoll gwatch_strcoll
#ifdef strerror
#undef strerror
#endif
#define strerror gwatch_strerror
#ifdef strncmp
#undef strncmp
#endif
#define strncmp(a, b, n) rb->strncmp((a), (b), (n))
#ifdef memchr
#undef memchr
#endif
#define memchr(s, c, n) rb->memchr((s), (c), (n))
#define srand(seed) rb->srand(seed)
#define rand() rb->rand()
#define vsnprintf(buf, size, fmt, ap) rb->vsnprintf((buf), (size), (fmt), (ap))
#define time gwatch_time
#define localtime gwatch_localtime
#define floor gwatch_floor
#define ceil gwatch_ceil
#define fmod gwatch_fmod
#define pow gwatch_pow
#define tan gwatch_tan
#define sqrt gwatch_sqrt
#define sin gwatch_sin
#define cos gwatch_cos
#define log10 gwatch_log10
#define log gwatch_log
#define exp gwatch_exp
#define atan2 gwatch_atan2
#define asin gwatch_asin
#define acos gwatch_acos
#define fabs gwatch_fabs
#define frexp gwatch_frexp

#endif
