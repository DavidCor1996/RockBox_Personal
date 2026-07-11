#ifndef GWATCH_PLATFORM_H
#define GWATCH_PLATFORM_H

#include "plugin.h"

#include <stddef.h>
#include <time.h>

bool gwatch_platform_init_memory(void);
void gwatch_platform_release_memory(void);
void *gwatch_malloc(size_t size);
void *gwatch_calloc(size_t nmemb, size_t size);
void *gwatch_realloc(void *ptr, size_t size);
void gwatch_free(void *ptr);

time_t gwatch_time(time_t *timer);
struct tm *gwatch_localtime(const time_t *timer);
char *gwatch_getenv(const char *name);
int gwatch_system(const char *command);
FILE *gwatch_fopen(const char *path, const char *mode);
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

void *gwrom_malloc(size_t size);
void *gwrom_realloc(void *ptr, size_t size);
void gwrom_free(void *ptr);
void *gwlua_malloc(size_t size);
void *gwlua_realloc(void *ptr, size_t size);
void gwlua_free(void *ptr);

#endif
