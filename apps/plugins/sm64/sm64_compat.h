#ifndef SM64_ROCKBOX_COMPAT_H
#define SM64_ROCKBOX_COMPAT_H

/* Included before every upstream translation unit.  The decompilation stays
 * untouched while its small libc surface is routed through the plugin API. */
#include "plugin.h"
#include "tlsf.h"
#include "sm64_rockbox.h"

/* Rockbox's target identifier and SM64's in-game model id share this legacy
 * macro name.  The target configuration is already consumed by plugin.h. */
#ifdef MODEL_NUMBER
#undef MODEL_NUMBER
#endif
#ifdef DEBUG_H
#undef DEBUG_H
#endif
#ifdef CONFIG_H
#undef CONFIG_H
#endif
#ifdef nop
#undef nop
#endif
#ifdef ABS
#undef ABS
#endif

#define TARGET_ROCKBOX 1

#define malloc  tlsf_malloc
#define calloc  tlsf_calloc
#define realloc tlsf_realloc
#define free    tlsf_free

#define memcpy  rb->memcpy
#define memmove rb->memmove
#define memset  rb->memset
#define memcmp  rb->memcmp
#define strlen  rb->strlen
#define strcmp  rb->strcmp
#define strncmp rb->strncmp
#define strcpy  rb->strcpy
#define strncpy rb->strncpy
#define strchr  rb->strchr
#define strrchr rb->strrchr
#define qsort   rb->qsort

#define printf  sm64_logf
#define puts    sm64_puts
#define sprintf sm64_sprintf
#define snprintf sm64_snprintf
#define abort() sm64_abort()
#define exit(status) sm64_exit(status)

#endif
