#include "nano_modules.h"
#include <limits.h>
#include <string.h>

struct module_io { struct nano_modules *manager; uint16_t id; };

static int read_module(void *context, uint32_t at, void *out, uint32_t size)
{
    struct module_io *io = context;
    return io->manager->read(io->manager->context, io->id, at, out, size);
}

int nano_modules_symbol(struct nano_modules *m, uint16_t id, uint64_t hash,
                        struct nano_module_symbol *out)
{
    struct module_io io = { m, id };
    if (!m || id >= m->count || !m->slots[id].references)
        return NANO_MODULE_IMPORT;
    return nano_module_export(read_module, &io, &m->slots[id].info,
                              m->slots[id].base, hash, out);
}

static int resolve_module(void *context, uint64_t hash,
                          struct nano_module_symbol *symbol)
{
    struct module_io *io = context;
    struct nano_modules *m = io->manager;
    const struct nano_module_desc *d = &m->descriptors[io->id];
    unsigned i;
    int rc;
    for (i = 0; i < d->dependency_count; i++) {
        rc = nano_modules_symbol(m, d->dependencies[i], hash, symbol);
        if (rc == NANO_MODULE_OK) return 0;
        if (rc != NANO_MODULE_IMPORT) return -1;
    }
    return m->external ? m->external(m->context, hash, symbol) : -1;
}

void nano_modules_release(struct nano_modules *m, uint16_t id)
{
    struct nano_module_slot *slot;
    const struct nano_module_desc *d;
    unsigned i;
    void *memory;
    if (!m || id >= m->count) return;
    slot = &m->slots[id];
    if (!slot->references || --slot->references) return;
    d = &m->descriptors[id];
    memory = slot->memory;
    /* No executable entry remains published while the allocation is freed. */
    memset(slot, 0, sizeof *slot);
    m->release(m->context, memory, d->memory_bytes);
    m->used -= d->memory_bytes;
    for (i = d->dependency_count; i; i--)
        nano_modules_release(m, d->dependencies[i - 1]);
}

static int acquire(struct nano_modules *m, uint16_t id, unsigned depth)
{
    struct nano_module_slot *slot;
    const struct nano_module_desc *d;
    struct module_io io = { m, id };
    unsigned held = 0;
    int rc;
    if (id >= m->count || depth > 32) return NANO_MODULE_FORMAT;
    slot = &m->slots[id]; d = &m->descriptors[id];
    if (slot->loading) return NANO_MODULE_FORMAT;
    if (slot->references) {
        if (slot->references == UINT32_MAX) return NANO_MODULE_CAPACITY;
        slot->references++;
        return NANO_MODULE_OK;
    }
    if (!d->memory_bytes || (d->dependency_count && !d->dependencies))
        return NANO_MODULE_FORMAT;
    slot->loading = 1;
    for (; held < d->dependency_count; held++) {
        rc = acquire(m, d->dependencies[held], depth + 1);
        if (rc) goto fail;
    }
    if (m->used > m->budget || d->memory_bytes > m->budget - m->used) {
        rc = NANO_MODULE_CAPACITY; goto fail;
    }
    /* Check the actual header against the generated descriptor before alloc. */
    rc = nano_module_inspect(read_module, &io, &slot->info);
    if (rc) goto fail;
    if (slot->info.memory_bytes != d->memory_bytes || slot->info.crc32 != d->crc32) {
        rc = NANO_MODULE_FORMAT; goto fail;
    }
    slot->memory = m->allocate(m->context, d->memory_bytes, &slot->base);
    if (!slot->memory) { rc = NANO_MODULE_CAPACITY; goto fail; }
    m->used += d->memory_bytes;
    if (m->used > m->peak) m->peak = m->used;
    rc = nano_module_load(read_module, &io, resolve_module, &io,
                          m->sync, m->context, slot->memory, d->memory_bytes,
                          slot->base, &slot->info);
    if (rc) {
        m->release(m->context, slot->memory, d->memory_bytes);
        m->used -= d->memory_bytes;
        goto fail;
    }
    slot->loading = 0;
    slot->references = 1;
    return NANO_MODULE_OK;
fail:
    memset(slot, 0, sizeof *slot);
    while (held) nano_modules_release(m, d->dependencies[--held]);
    return rc;
}

int nano_modules_acquire(struct nano_modules *m, uint16_t id)
{
    if (!m || !m->descriptors || !m->slots || !m->read || !m->allocate ||
        !m->release || !m->sync) return NANO_MODULE_FORMAT;
    return acquire(m, id, 0);
}
