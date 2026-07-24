#ifndef POCKETSKY_RB_COMPAT_H
#define POCKETSKY_RB_COMPAT_H

#include "plugin.h"
#include <tlsf.h>

int ps_snprintf(char *buffer, size_t size, const char *format, ...);
int ps_fprintf(void *stream, const char *format, ...);
void ps_exit(int status);
void *ps_memcpy(void *destination, const void *source, size_t size);
void *ps_memset(void *destination, int value, size_t size);
int ps_strcmp(const char *left, const char *right);
char *ps_strcpy(char *destination, const char *source);
size_t ps_strlen(const char *text);

#define calloc tlsf_calloc
#define free tlsf_free
#define malloc tlsf_malloc
#define realloc tlsf_realloc
#define memcpy ps_memcpy
#define memset ps_memset
#define strcmp ps_strcmp
#define strcpy ps_strcpy
#define strlen ps_strlen
#define snprintf ps_snprintf
#define fprintf ps_fprintf
#define exit ps_exit
#ifndef stderr
#define stderr ((void *)0)
#endif

#ifndef ASTRONOMY_ENGINE_NO_CURRENT_TIME
#define ASTRONOMY_ENGINE_NO_CURRENT_TIME
#endif

#endif
