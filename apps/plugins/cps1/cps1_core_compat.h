#ifndef ROCKBOX_CPS1_CORE_COMPAT_H
#define ROCKBOX_CPS1_CORE_COMPAT_H

#ifdef __cplusplus
#include "lib/plugin_cxx_compat.h"
#else
#include "plugin.h"
#endif
#include "cps1.h"
#include "lib/stdio_compat.h"
#include <stddef.h>
#include <stdint.h>
#include <math.h>

typedef char TCHAR;
#ifndef SIMULATOR
typedef long clock_t;
#endif

#ifdef CPU
#undef CPU
#endif

#ifndef __fastcall
#define __fastcall
#endif
#ifndef __cdecl
#define __cdecl
#endif
#ifndef _T
#define _T(value) value
#endif
#ifndef _tcslen
#define _tcslen(value) strlen(value)
#endif

#define malloc cps1_malloc
#define calloc cps1_calloc
#define realloc cps1_realloc
#define free cps1_free
#define abort cps1_abort
#define printf cps1_printf
#define sprintf cps1_sprintf
#ifdef fprintf
#undef fprintf
#endif
#define fprintf(...) ((int)0)
#define puts(...) ((int)0)
#define assert(value) do { if (!(value)) cps1_abort(); } while (0)

#define memcpy rb->memcpy
#define memmove rb->memmove
#define memset rb->memset
#define memcmp rb->memcmp
#define strlen rb->strlen
#define strcmp rb->strcmp
#define strncmp rb->strncmp
#define strcasecmp rb->strcasecmp
#define strcpy rb->strcpy
#define strncpy rb->strncpy
#define strcat rb->strcat
#define strchr rb->strchr
#define strrchr rb->strrchr
#define strstr rb->strstr
#define wcslen cps1_wcslen
#define wcstombs cps1_wcstombs

#define sin cps1_sin
#define sqrt cps1_sqrt
#define log cps1_log
#define pow cps1_pow
#define floor cps1_floor

#endif
