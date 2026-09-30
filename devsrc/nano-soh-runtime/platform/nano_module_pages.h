#ifndef NANO_MODULE_PAGES_H
#define NANO_MODULE_PAGES_H
#include <stdint.h>

/* Adapts prefix-only whole-file I/O to module range reads using one page.
 * read_file gets "resident/000000.nmp" or "actor-NNN/000000.nmp".
 * A NanoApps adapter prepends its fixed installation directory. */
struct nano_module_pages {
    int (*read_file)(void *, const char *, void *, uint32_t, uint32_t *);
    void *context;
    const char *const *names;
    uint16_t count, cached_module;
    uint32_t cached_page, cached_bytes;
    uint8_t valid;
    uint8_t bytes[65536];
};

int nano_module_pages_read(void *, uint16_t, uint32_t, void *, uint32_t);
void nano_module_pages_invalidate(struct nano_module_pages *);
#endif
