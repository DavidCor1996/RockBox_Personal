#ifndef OPENLARA_ROCKBOX_COMPAT_H
#define OPENLARA_ROCKBOX_COMPAT_H

#include "lib/plugin_cxx_compat.h"

#ifdef swap16
#undef swap16
#endif
#ifdef swap32
#undef swap32
#endif
#ifdef abs
#undef abs
#endif

#define memcpy  rb->memcpy
#define memmove rb->memmove
#define memset  rb->memset
#define memcmp  rb->memcmp
#define strlen  rb->strlen

#endif
