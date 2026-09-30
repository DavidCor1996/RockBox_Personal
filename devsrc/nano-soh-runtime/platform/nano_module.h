#ifndef NANO_MODULE_H
#define NANO_MODULE_H

#include <stdint.h>

/* NSM1 is a native ARM module, not an N64 overlay or an executable hbapp.
 * All storage and RAM are caller-owned. Loading never executes module code. */
enum nano_module_status {
    NANO_MODULE_OK, NANO_MODULE_IO, NANO_MODULE_FORMAT, NANO_MODULE_CHECKSUM,
    NANO_MODULE_CAPACITY, NANO_MODULE_IMPORT, NANO_MODULE_RELOCATION,
    NANO_MODULE_SYNC
};

struct nano_module_symbol {
    uint32_t address;
    uint32_t thumb;
};

struct nano_module_info {
    uint32_t image_bytes, memory_bytes, file_bytes, exports, export_offset;
    uint32_t ready, crc32;
};

/* read must provide exactly length bytes, or fail. A target adapter may use
 * bounded page files; it must not load a second copy of the entire module. */
typedef int (*nano_module_read)(void *, uint32_t, void *, uint32_t);
typedef int (*nano_module_resolve)(void *, uint64_t,
                                    struct nano_module_symbol *);
/* Mandatory target cache synchronization, after ALL writes, before publish.
 * Host tests can provide a no-op; hardware must provide the real operation. */
typedef int (*nano_module_sync)(void *, void *, uint32_t);

int nano_module_inspect(nano_module_read, void *, struct nano_module_info *);
int nano_module_load(nano_module_read, void *, nano_module_resolve, void *,
                     nano_module_sync, void *, void *, uint32_t, uint32_t,
                     struct nano_module_info *);
int nano_module_export(nano_module_read, void *,
                       const struct nano_module_info *, uint32_t, uint64_t,
                       struct nano_module_symbol *);

#endif
