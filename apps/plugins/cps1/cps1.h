#ifndef ROCKBOX_CPS1_H
#define ROCKBOX_CPS1_H

#ifdef __cplusplus
#include "lib/plugin_cxx_compat.h"
#else
#include "plugin.h"
#endif
#include <stddef.h>
#include <stdint.h>

#define CPS1_ROOT ROCKBOX_DIR "/games/cps1"
#define CPS1_ROM_ROOT CPS1_ROOT "/roms"

#ifdef __cplusplus
extern "C" {
#endif

void *cps1_malloc(size_t size);
void *cps1_calloc(size_t count, size_t size);
void *cps1_realloc(void *pointer, size_t size);
void cps1_free(void *pointer);
void cps1_abort(void);
bool cps1_platform_allocation_failed(void);
void cps1_platform_reset_failure(void);
size_t cps1_platform_last_allocation_size(void);
int cps1_printf(const char *format, ...);
int cps1_sprintf(char *output, const char *format, ...);
double cps1_sin(double value);
double cps1_sqrt(double value);
double cps1_log(double value);
double cps1_pow(double base, double exponent);
double cps1_floor(double value);
size_t cps1_wcslen(const wchar_t *value);
size_t cps1_wcstombs(char *output, const wchar_t *input, size_t size);

int cps1_inflate_raw(void *output, size_t output_size,
                     const void *input, size_t input_size);

#ifdef __cplusplus
}
#endif

#endif
