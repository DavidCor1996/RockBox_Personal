/***************************************************************************
 * Compatibility surface injected before each upstream Zelda3 source file.
 ****************************************************************************/
#ifndef ZELDA3_ROCKBOX_COMPAT_H
#define ZELDA3_ROCKBOX_COMPAT_H

#include "plugin.h"
#include "tlsf.h"
#include "zelda3.h"

/* Names used by both Rockbox and the SNES DSP are kept distinct after the
 * Rockbox headers have been consumed. */
#define dsp_init zelda3_dsp_init
#define DIR ZELDA3_DSP_DIR

#ifndef SIMULATOR
typedef void FILE;
#endif

#define ROCKBOX 1
#define TARGET_ROCKBOX 1

#define malloc  tlsf_malloc
#define calloc  tlsf_calloc
#define realloc tlsf_realloc
#define free    tlsf_free

#define memcpy  rb->memcpy
#define memmove rb->memmove
#define memset  rb->memset
#define memcmp  rb->memcmp
#define memchr  rb->memchr
#define strlen  rb->strlen
#define strcmp  rb->strcmp
#define strncmp rb->strncmp
#define strchr  rb->strchr
#define strrchr rb->strrchr
#define strdup  zelda3_strdup

#define printf   zelda3_printf
#define puts     zelda3_puts
#define fprintf  zelda3_fprintf
#define sprintf  zelda3_sprintf
#define snprintf zelda3_snprintf
#define vsnprintf zelda3_vsnprintf
#define fopen    zelda3_fopen
#define fread    zelda3_fread
#define fwrite   zelda3_fwrite
#define fseek    zelda3_fseek
#define ftell    zelda3_ftell
#define fclose   zelda3_fclose
#define rename   zelda3_rename
#define rewind(stream) zelda3_fseek((stream), 0, SEEK_SET)
#define setvbuf(stream, buffer, mode, size) (0)
#define abort()  zelda3_abort()
#define exit(s)  zelda3_exit(s)
#ifdef stderr
#undef stderr
#endif
#ifdef stdout
#undef stdout
#endif
#define stderr NULL
#define stdout NULL

#ifndef SEEK_SET
#define SEEK_SET 0
#define SEEK_CUR 1
#define SEEK_END 2
#endif
#ifndef _IOFBF
#define _IOFBF 0
#endif

#endif
