/* Fixed original actor registry; no mod registry or C++ containers. */
#include "global.h"
#include "soh/ActorDB.h"
#include <string.h>
#ifdef SHIP_NANO_OVERLAYS
#include "nano_actor_modules.h"

static nano_actor_acquire acquire_module;
static nano_actor_release release_module;
static void *module_context;
static unsigned module_owners;
static uint8_t retired[ACTOR_ID_MAX];
#else

#define DEFINE_ACTOR(name, id, alloc) extern ActorInit name##_InitVars;
#define DEFINE_ACTOR_INTERNAL DEFINE_ACTOR
#define DEFINE_ACTOR_UNSET(id)
#include "tables/actor_table.h"
#undef DEFINE_ACTOR
#undef DEFINE_ACTOR_INTERNAL
#undef DEFINE_ACTOR_UNSET

#define DEFINE_ACTOR(name, id, alloc) [id] = &name##_InitVars,
#define DEFINE_ACTOR_INTERNAL DEFINE_ACTOR
#define DEFINE_ACTOR_UNSET(id) [id] = NULL,
static ActorInit *const initial[ACTOR_ID_MAX] = {
#include "tables/actor_table.h"
};
#undef DEFINE_ACTOR
#undef DEFINE_ACTOR_INTERNAL
#undef DEFINE_ACTOR_UNSET

#endif
#define DEFINE_ACTOR(name, id, alloc) [id] = #name,
#define DEFINE_ACTOR_INTERNAL DEFINE_ACTOR
#define DEFINE_ACTOR_UNSET(id) [id] = NULL,
static const char *const names[ACTOR_ID_MAX] = {
#include "tables/actor_table.h"
};
#undef DEFINE_ACTOR
#undef DEFINE_ACTOR_INTERNAL
#undef DEFINE_ACTOR_UNSET

static ActorDBEntry entries[ACTOR_ID_MAX];
static ActorDBEntry invalid;

ActorDBEntry *ActorDB_Retrieve(const int id)
{
    ActorDBEntry *entry;
    const ActorInit *init;
    if (id < 0 || id >= ACTOR_ID_MAX || !names[id]) return &invalid;
    entry = &entries[id];
#ifdef SHIP_NANO_OVERLAYS
    retired[id] = 0;
#endif
    if (!entry->valid) {
#ifdef SHIP_NANO_OVERLAYS
        if (!acquire_module) return &invalid;
        init = acquire_module(module_context, id, names[id]);
        if (!init) return &invalid;
        if (init->id != id || !init->instanceSize) {
            release_module(module_context, id);
            return &invalid;
        }
        module_owners++;
#else
        init = initial[id];
#endif
        entry->name = names[id];
        entry->desc = names[id];
        entry->id = id;
        entry->category = init->category;
        entry->flags = init->flags;
        entry->objectId = init->objectId;
        entry->instanceSize = init->instanceSize;
        entry->init = init->init;
        entry->destroy = init->destroy;
        entry->update = init->update;
        entry->draw = init->draw;
        entry->reset = init->reset;
        entry->numLoaded = 0;
        entry->valid = 1;
    }
    return entry;
}

int ActorDB_RetrieveId(const char *name)
{
    int id;
    if (name)
        for (id = 0; id < ACTOR_ID_MAX; id++)
            if (names[id] && !strcmp(names[id], name)) return id;
    return -1;
}

/* Called only after the game has destroyed every live actor. */
void nano_actor_db_reset(void)
{
#ifdef SHIP_NANO_OVERLAYS
    unsigned id;
    /* Refuse reset while callers still hold actors/callbacks. */
    for (id = 0; id < ACTOR_ID_MAX; id++)
        if (entries[id].numLoaded) return;
    for (id = 0; id < ACTOR_ID_MAX; id++)
        nano_actor_module_release((int)id);
    /* A scene reset is not itself proof of GPU completion. */
    return;
#endif
    memset(entries, 0, sizeof entries);
    memset(&invalid, 0, sizeof invalid);
}

#ifdef SHIP_NANO_OVERLAYS
int nano_actor_modules_configure(nano_actor_acquire acquire,
                                 nano_actor_release release, void *context)
{
    if (module_owners || (!!acquire != !!release)) return 0;
    acquire_module = acquire;
    release_module = release;
    module_context = context;
    return 1;
}

void nano_actor_module_release(int id)
{
    ActorDBEntry *entry;
    if (id < 0 || id >= ACTOR_ID_MAX) return;
    entry = &entries[id];
    if (!entry->valid || entry->numLoaded) return;
    retired[id] = 1;
}

void nano_actor_modules_collect(void)
{
    unsigned id;
    for (id = 0; id < ACTOR_ID_MAX; id++) {
        ActorDBEntry *entry = &entries[id];
        if (!retired[id] || !entry->valid || entry->numLoaded) continue;
        /* No executable entry remains published when ownership is released. */
        retired[id] = 0;
        memset(entry, 0, sizeof *entry);
        module_owners--;
        release_module(module_context, (int)id);
    }
}
#endif
