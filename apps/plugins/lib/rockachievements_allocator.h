#ifndef ROCKACHIEVEMENTS_ALLOCATOR_H
#define ROCKACHIEVEMENTS_ALLOCATOR_H

#include <stddef.h>

void rockachievements_allocator_reset(void *buffer, size_t size);
void *rockachievements_malloc(size_t size);
void *rockachievements_calloc(size_t count, size_t size);
void *rockachievements_realloc(void *pointer, size_t size);
void rockachievements_free(void *pointer);

#endif

