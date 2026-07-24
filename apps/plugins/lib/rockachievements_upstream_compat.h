#ifndef ROCKACHIEVEMENTS_UPSTREAM_COMPAT_H
#define ROCKACHIEVEMENTS_UPSTREAM_COMPAT_H

#define RC_NO_THREADS 1
#define NDEBUG 1

#include "plugin.h"
#include "rockachievements_allocator.h"
#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

#ifndef INFINITY
#define INFINITY (__builtin_inff())
#endif
#ifndef NAN
#define NAN (__builtin_nanf(""))
#endif

double fmod(double value, double divisor);

#define malloc rockachievements_malloc
#define calloc rockachievements_calloc
#define realloc rockachievements_realloc
#define free rockachievements_free
#define strtoul rb->strtoul

#endif
