#ifndef ROCKBOX_PICODRIVE_CORE_COMPAT_H
#define ROCKBOX_PICODRIVE_CORE_COMPAT_H

#include "plugin.h"
#include "picodrive.h"
#include "lib/stdio_compat.h"
#include <stddef.h>
#include <stdint.h>
#include <math.h>

/* Rockbox exposes CPU as a target-selection macro. PicoDrive's bundled
 * Z80 cores use CPU as a type/argument name, after plugin.h has finished
 * consuming it the macro must not leak into those headers. */
#ifdef CPU
#undef CPU
#endif

void pd_abort(void);
int pd_printf(const char *format, ...);
long pd_strtol(const char *text, char **end, int base);
unsigned long pd_strtoul(const char *text, char **end, int base);
int pd_atoi(const char *text);
char *pd_strdup(const char *text);
char *pd_strncpy(char *destination, const char *source, size_t count);
int pd_abs(int value);
double pd_sin(double value);
double pd_cos(double value);
double pd_sqrt(double value);
double pd_fabs(double value);
double pd_log_math(double value);
double pd_pow(double base, double exponent);
double pd_floor(double value);
double pd_round(double value);

#ifdef NO_SMS
void PicoPrepareMS(void);
#endif

#define malloc pd_malloc
#define calloc pd_calloc
#define realloc pd_realloc
#define free pd_free
#define abort pd_abort
#define printf pd_printf
#define puts(...) ((int)0)
#define strtol pd_strtol
#define strtoul pd_strtoul
#define atoi pd_atoi
#define strdup pd_strdup
#undef abs
#define abs pd_abs
#define sin pd_sin
#define cos pd_cos
#define sqrt pd_sqrt
#define fabs pd_fabs
#define log pd_log_math
#define pow pd_pow
#define floor pd_floor
#define round pd_round
#define rand() rb->rand()
#define srand(seed) rb->srand(seed)

#define memcpy rb->memcpy
#define memmove rb->memmove
#define memset rb->memset
#define memcmp rb->memcmp
#define strlen rb->strlen
#define strcmp rb->strcmp
#define strncmp rb->strncmp
#define strcasecmp rb->strcasecmp
#define strncasecmp rb->strncasecmp
#define strcpy rb->strcpy
#define strncpy pd_strncpy
#define strcat rb->strcat
#define strchr rb->strchr
#define strrchr rb->strrchr
#define strstr rb->strstr

#endif
