/***************************************************************************
 *             __________               __   ___.
 *   Open      \______   \ ____   ____ |  | _\_ |__   _______  ___
 *   Source     |       _//  _ \_/ ___\|  |/ /| __ \ /  _ \  \/  /
 *   Jukebox    |    |   (  <_> )  \___|    < | \_\ (  <_> > <  <
 *   Firmware   |____|_  /\____/ \___  >__|_ \|___  /\____/__/\_ \
 *                     \/            \/     \/    \/                \/
 *
 * Minimal C++ runtime hooks for plugins.
 *
 ****************************************************************************/

#include "plugin_cxx_compat.h"

extern "C" {
#include "plugin_cxx.h"
}

static bool cxx_ready;

struct cxx_block {
    size_t size;
    bool free;
    cxx_block *next;
};

static cxx_block *cxx_head;
static uintptr_t cxx_start;
static uintptr_t cxx_end;

static size_t cxx_block_overhead(void)
{
    return (sizeof(cxx_block) + 15U) & ~(size_t)15U;
}

static void *cxx_payload(cxx_block *block)
{
    return (unsigned char *)block + cxx_block_overhead();
}

static cxx_block *cxx_payload_block(void *ptr)
{
    return (cxx_block *)((unsigned char *)ptr - cxx_block_overhead());
}

static bool cxx_block_in_heap(cxx_block *block)
{
    uintptr_t pos = (uintptr_t)block;
    return pos >= cxx_start && pos + cxx_block_overhead() <= cxx_end;
}

extern "C" void plugin_cxx_init(void *buffer, size_t buffer_size)
{
    uintptr_t start = ((uintptr_t)buffer + 15U) & ~(uintptr_t)15U;
    uintptr_t end = ((uintptr_t)buffer + buffer_size) & ~(uintptr_t)15U;
    size_t overhead = cxx_block_overhead();
    cxx_head = NULL;
    cxx_start = start;
    cxx_end = end;
    if (end > start + overhead)
    {
        cxx_head = (cxx_block *)start;
        cxx_head->size = end - start - overhead;
        cxx_head->free = true;
        cxx_head->next = NULL;
    }
    cxx_ready = cxx_head != NULL;
}

extern "C" size_t plugin_cxx_available(void)
{
    size_t total = 0;
    cxx_block *block;

    if (!cxx_ready)
        return 0;

    for (block = cxx_head; block; block = block->next)
    {
        if (!cxx_block_in_heap(block))
            break;
        if (block->free)
            total += block->size;
    }
    return total;
}

static void *plugin_cxx_alloc(size_t size)
{
    cxx_block *block;
    size_t overhead = cxx_block_overhead();
    unsigned int guard = 0;

    if (!cxx_ready || size == 0)
        return 0;

    size = (size + 15U) & ~(size_t)15U;
    for (block = cxx_head; block; block = block->next)
    {
        if (++guard > 262144 || !cxx_block_in_heap(block))
            return 0;

        if (!block->free || block->size < size)
            continue;

        if (block->size >= size + overhead + 16U)
        {
            cxx_block *split = (cxx_block *)
                ((unsigned char *)cxx_payload(block) + size);
            split->size = block->size - size - overhead;
            split->free = true;
            split->next = block->next;
            block->next = split;
            block->size = size;
        }

        block->free = false;
        return cxx_payload(block);
    }

    return 0;
}

static void plugin_cxx_free(void *ptr)
{
    cxx_block *block;
    size_t overhead = cxx_block_overhead();

    if (!ptr)
        return;

    if ((uintptr_t)ptr < cxx_start + overhead || (uintptr_t)ptr >= cxx_end)
        return;

    block = cxx_payload_block(ptr);
    if (!cxx_block_in_heap(block))
        return;

    block->free = true;

    for (block = cxx_head; block && block->next; block = block->next)
    {
        if (!cxx_block_in_heap(block) || !cxx_block_in_heap(block->next))
            break;

        unsigned char *block_end = (unsigned char *)cxx_payload(block) + block->size;
        if (block->free && block->next->free &&
            block_end == (unsigned char *)block->next)
        {
            block->size += overhead + block->next->size;
            block->next = block->next->next;
        }
    }
}

void *operator new(size_t size) throw()
{
    return plugin_cxx_alloc(size);
}

void *operator new[](size_t size) throw()
{
    return plugin_cxx_alloc(size);
}

void operator delete(void *ptr) throw()
{
    plugin_cxx_free(ptr);
}

void operator delete[](void *ptr) throw()
{
    plugin_cxx_free(ptr);
}

void operator delete(void *ptr, size_t) throw()
{
    plugin_cxx_free(ptr);
}

void operator delete[](void *ptr, size_t) throw()
{
    plugin_cxx_free(ptr);
}
