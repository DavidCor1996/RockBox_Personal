/*
 * Rockbox-local configuration for selected ScummVM 1.9.0 sources.
 * This is intentionally minimal and grows as upstream files are enabled.
 */

#ifndef SCUMMVM_ROCKBOX_UPSTREAM_CONFIG_H
#define SCUMMVM_ROCKBOX_UPSTREAM_CONFIG_H

#define SCUMM_LITTLE_ENDIAN
#define HAVE_INTTYPES_H
#define HAVE_STDINT_H
#define HAVE_INT64

#define DISABLE_TEXT_CONSOLE
#define DISABLE_FANCY_THEMES
#define DISABLE_DEFAULT_SAVEFILEMANAGER
#define DISABLE_DOSBOX_OPL
#define DISABLE_NUKED_OPL

typedef unsigned char byte;
typedef unsigned int uint;
typedef unsigned char uint8;
typedef unsigned short uint16;
typedef unsigned int uint32;
typedef signed char int8;
typedef signed short int16;
typedef signed int int32;

typedef signed long long int64;
typedef unsigned long long uint64;

#endif
