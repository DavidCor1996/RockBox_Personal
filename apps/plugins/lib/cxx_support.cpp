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

extern "C" void plugin_cxx_init(void *buffer, size_t buffer_size)
{
    uintptr_t start = ((uintptr_t)buffer + 15U) & ~(uintptr_t)15U;
    uintptr_t end = ((uintptr_t)buffer + buffer_size) & ~(uintptr_t)15U;
    cxx_head = NULL;
    if (end > start + sizeof(cxx_block))
    {
        cxx_head = (cxx_block *)start;
        cxx_head->size = end - start - sizeof(cxx_block);
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
        if (block->free)
            total += block->size;
    }
    return total;
}

static void *plugin_cxx_alloc(size_t size)
{
    cxx_block *block;

    if (!cxx_ready || size == 0)
        return 0;

    size = (size + 15U) & ~(size_t)15U;
    for (block = cxx_head; block; block = block->next)
    {
        if (!block->free || block->size < size)
            continue;

        if (block->size >= size + sizeof(cxx_block) + 16U)
        {
            cxx_block *split = (cxx_block *)
                ((unsigned char *)(block + 1) + size);
            split->size = block->size - size - sizeof(cxx_block);
            split->free = true;
            split->next = block->next;
            block->next = split;
            block->size = size;
        }

        block->free = false;
        return block + 1;
    }

    return 0;
}

static void plugin_cxx_free(void *ptr)
{
    cxx_block *block;

    if (!ptr)
        return;

    block = ((cxx_block *)ptr) - 1;
    block->free = true;

    for (block = cxx_head; block && block->next; block = block->next)
    {
        unsigned char *block_end = (unsigned char *)(block + 1) + block->size;
        if (block->free && block->next->free &&
            block_end == (unsigned char *)block->next)
        {
            block->size += sizeof(cxx_block) + block->next->size;
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
