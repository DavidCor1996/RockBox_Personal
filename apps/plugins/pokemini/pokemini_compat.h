#ifndef ROCKBOX_POKEMINI_COMPAT_H
#define ROCKBOX_POKEMINI_COMPAT_H

#include "plugin.h"
#include <stddef.h>

void *pm_malloc(size_t size);
void *pm_calloc(size_t nmemb, size_t size);
void pm_free(void *ptr);
long pm_time(void *timer);

#define malloc pm_malloc
#define calloc pm_calloc
#define free pm_free
#define strcpy rb->strcpy
#define strcmp rb->strcmp
#define strlen rb->strlen
#define memcpy rb->memcpy
#define memset rb->memset
#define time pm_time

#endif
