#include "plugin.h"
#include "rockachievements_allocator.h"

struct ra_block
{
    size_t size;
    struct ra_block *next;
    bool free;
};

static struct ra_block *ra_first;

static size_t align_size(size_t size)
{
    return (size + sizeof(void *) - 1) & ~(sizeof(void *) - 1);
}

void rockachievements_allocator_reset(void *buffer, size_t size)
{
    uintptr_t start = ((uintptr_t)buffer + sizeof(void *) - 1) &
                      ~(uintptr_t)(sizeof(void *) - 1);
    size_t skipped = start - (uintptr_t)buffer;

    ra_first = NULL;
    if (!buffer || size <= skipped + sizeof(struct ra_block))
        return;
    ra_first = (struct ra_block *)start;
    ra_first->size = size - skipped - sizeof(*ra_first);
    ra_first->next = NULL;
    ra_first->free = true;
}

void *rockachievements_malloc(size_t size)
{
    struct ra_block *block;

    if (!ra_first)
        return NULL;
    size = align_size(MAX(size, (size_t)1));
    for (block = ra_first; block; block = block->next)
    {
        if (!block->free || block->size < size)
            continue;
        if (block->size >= size + sizeof(*block) + sizeof(void *))
        {
            struct ra_block *next = (struct ra_block *)
                ((unsigned char *)(block + 1) + size);
            next->size = block->size - size - sizeof(*next);
            next->next = block->next;
            next->free = true;
            block->next = next;
            block->size = size;
        }
        block->free = false;
        return block + 1;
    }
    return NULL;
}

void *rockachievements_calloc(size_t count, size_t size)
{
    size_t total;
    void *result;

    if (count && size > (size_t)-1 / count)
        return NULL;
    total = count * size;
    result = rockachievements_malloc(total);
    if (result)
        rb->memset(result, 0, total);
    return result;
}

void rockachievements_free(void *pointer)
{
    struct ra_block *block;
    struct ra_block *previous = NULL;

    if (!pointer)
        return;
    block = (struct ra_block *)pointer - 1;
    block->free = true;
    for (block = ra_first; block; previous = block, block = block->next)
    {
        while (block->free && block->next && block->next->free)
        {
            block->size += sizeof(*block) + block->next->size;
            block->next = block->next->next;
        }
        if (previous && previous->free && block->free)
        {
            previous->size += sizeof(*block) + block->size;
            previous->next = block->next;
            block = previous;
        }
    }
}

void *rockachievements_realloc(void *pointer, size_t size)
{
    struct ra_block *block;
    void *replacement;

    if (!pointer)
        return rockachievements_malloc(size);
    if (!size)
    {
        rockachievements_free(pointer);
        return NULL;
    }
    block = (struct ra_block *)pointer - 1;
    if (block->size >= size)
        return pointer;
    replacement = rockachievements_malloc(size);
    if (!replacement)
        return NULL;
    rb->memcpy(replacement, pointer, block->size);
    rockachievements_free(pointer);
    return replacement;
}

