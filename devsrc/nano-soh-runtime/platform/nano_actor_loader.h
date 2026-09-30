#ifndef NANO_ACTOR_LOADER_H
#define NANO_ACTOR_LOADER_H
#include "nano_modules.h"

struct nano_actor_location {
    uint32_t profile_offset;
    uint16_t module, valid;
};

struct nano_actor_loader {
    struct nano_modules *modules;
    const struct nano_actor_location *actors;
    unsigned count;
};

/* Binding storage must outlive every actor and pending GPU retirement. The
 * caller must separately retain the resident core for the whole game session. */
int nano_actor_loader_bind(struct nano_actor_loader *);
#endif
