#ifndef NANO_MODULES_H
#define NANO_MODULES_H
#include "nano_module.h"

struct nano_module_desc {
    uint32_t memory_bytes, crc32;
    const uint16_t *dependencies;
    uint16_t dependency_count;
};

struct nano_module_slot {
    void *memory;
    uint32_t base, references;
    struct nano_module_info info;
    uint8_t loading;
};

/* One UI/application thread owns this manager. Callbacks must not re-enter.
 * All descriptors, slots and the byte budget are supplied by the caller. */
struct nano_modules {
    const struct nano_module_desc *descriptors;
    struct nano_module_slot *slots;
    uint16_t count;
    uint32_t budget, used, peak;
    void *context;
    int (*read)(void *, uint16_t, uint32_t, void *, uint32_t);
    void *(*allocate)(void *, uint32_t, uint32_t *);
    void (*release)(void *, void *, uint32_t);
    nano_module_resolve external;
    nano_module_sync sync;
};

/* Zero-initialize the manager's slots before first use. An acquired reference
 * pins the complete dependency closure. Release only after all CPU callbacks
 * and GPU uses of module data have stopped; this manager cannot prove that. */
int nano_modules_acquire(struct nano_modules *, uint16_t);
void nano_modules_release(struct nano_modules *, uint16_t);
int nano_modules_symbol(struct nano_modules *, uint16_t, uint64_t,
                        struct nano_module_symbol *);

#endif
