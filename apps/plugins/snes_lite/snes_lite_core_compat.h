/***************************************************************************
 * Compatibility surface kept local to the imported Snes9x2002 core.
 ***************************************************************************/

#ifndef SNES_LITE_CORE_COMPAT_H
#define SNES_LITE_CORE_COMPAT_H

#include "plugin.h"
#include <stddef.h>

/* Rockbox target constants collide with historical Snes9x identifiers. */
#ifdef CPU
#undef CPU
#endif
#ifdef RGB565
#undef RGB565
#endif

void *snes_lite_malloc(size_t size);
void *snes_lite_calloc(size_t count, size_t size);
void snes_lite_free(void *ptr);
void snes_lite_core_abort(int status);
int snes_lite_sprintf(char *buffer, const char *format, ...);
long snes_lite_strtol(const char *text, char **end, int base);
char *snes_lite_strncpy(char *destination, const char *source, size_t count);
int snes_lite_sscanf(const char *text, const char *format, ...);
void *snes_lite_bsearch(const void *key, const void *base, size_t count,
                        size_t size, int (*compare)(const void *,
                                                   const void *));
long snes_lite_time(long *value);
float sinf(float value);
float cosf(float value);
float tanf(float value);
float sqrtf(float value);

#define malloc snes_lite_malloc
#define calloc snes_lite_calloc
#define free snes_lite_free
#define exit snes_lite_core_abort
#define sprintf snes_lite_sprintf
#define strtol snes_lite_strtol
#define printf(...) ((int)0)
#define fprintf(...) ((int)0)
#define fflush(...) ((int)0)
#define sscanf snes_lite_sscanf
#define bsearch snes_lite_bsearch
#define time snes_lite_time
#define rand() rb->rand()

#define memcpy rb->memcpy
#define memmove rb->memmove
#define memset rb->memset
#define memcmp rb->memcmp
#define strlen rb->strlen
#define strcmp rb->strcmp
#define strncmp rb->strncmp
#define strcpy rb->strcpy
#define strncpy snes_lite_strncpy
#define strchr rb->strchr
#define strrchr rb->strrchr

#endif
