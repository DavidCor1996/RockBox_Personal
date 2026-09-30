#include "nano_actor_loader.h"
#include "nano_actor_modules.h"

static const ActorInit *acquire(void *context, int id, const char *name)
{
    struct nano_actor_loader *loader = context;
    const struct nano_actor_location *actor;
    struct nano_module_slot *slot;
    (void)name;
    if (id < 0 || (unsigned)id >= loader->count) return NULL;
    actor = &loader->actors[id];
    if (!actor->valid || actor->module >= loader->modules->count) return NULL;
    if (nano_modules_acquire(loader->modules, actor->module)) return NULL;
    slot = &loader->modules->slots[actor->module];
    if ((actor->profile_offset & 3) || slot->info.memory_bytes < sizeof(ActorInit) ||
        actor->profile_offset > slot->info.memory_bytes - sizeof(ActorInit)) {
        nano_modules_release(loader->modules, actor->module);
        return NULL;
    }
    return (const ActorInit *)((const uint8_t *)slot->memory + actor->profile_offset);
}

static void release(void *context, int id)
{
    struct nano_actor_loader *loader = context;
    nano_modules_release(loader->modules, loader->actors[id].module);
}

int nano_actor_loader_bind(struct nano_actor_loader *loader)
{
    if (!loader || !loader->modules || !loader->actors) return 0;
    return nano_actor_modules_configure(acquire, release, loader);
}
