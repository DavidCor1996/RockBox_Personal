#include "global.h"
#include "soh/ActorDB.h"
#include "nano_actor_modules.h"
#include "nano_actor_loader.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static uint8_t file_data[256];
static struct nano_actor_location actors[ACTOR_ID_MAX];

static void put(uint32_t at, uint32_t n)
{
    unsigned i;
    for (i = 0; i < 4; i++) file_data[at + i] = (uint8_t)(n >> (8 * i));
}

static int read_data(void *unused, uint16_t id, uint32_t at, void *out, uint32_t n)
{
    (void)unused;
    if (id || at > sizeof file_data || n > sizeof file_data - at) return -1;
    memcpy(out, file_data + at, n);
    return 0;
}

static void *allocate(void *unused, uint32_t n, uint32_t *base)
{
    (void)unused;
    *base = 0x09000000;
    return aligned_alloc(16, n);
}

static void release(void *unused, void *p, uint32_t size)
{
    (void)unused; (void)size;
    free(p);
}

static int sync_code(void *unused, void *p, uint32_t size)
{
    (void)unused; (void)p; (void)size;
    return 0;
}

int main(void)
{
    ActorInit profile = {0};
    ActorDBEntry *entry;
    struct nano_module_desc desc = {0};
    struct nano_module_slot slot = {0};
    struct nano_modules modules = {0};
    struct nano_actor_loader loader = { &modules, actors, ACTOR_ID_MAX };
    uint32_t image_size = ((uint32_t)sizeof profile + 15u) & ~15u;
    uint32_t crc = UINT32_MAX, i, bit;
    profile.id = ACTOR_EN_KO;
    profile.instanceSize = sizeof(Actor);
    profile.flags = 0x24;
    put(0, 0x314d534e); put(4, 1); put(8, 48 + image_size);
    put(12, image_size); put(20, 16);
    memcpy(file_data + 48, &profile, sizeof profile);
    for (i = 0; i < 48 + image_size; i++) {
        crc ^= file_data[i];
        for (bit = 0; bit < 8; bit++) crc = (crc >> 1) ^ (0xedb88320u & (0u - (crc & 1)));
    }
    put(36, ~crc);
    desc.memory_bytes = image_size; desc.crc32 = ~crc;
    modules.descriptors = &desc; modules.slots = &slot; modules.count = 1;
    modules.budget = image_size;
    modules.read = read_data; modules.allocate = allocate;
    modules.release = release; modules.sync = sync_code;
    actors[ACTOR_EN_KO].valid = 1;
    assert(nano_actor_loader_bind(&loader));
    entry = ActorDB_Retrieve(ACTOR_EN_KO);
    assert(entry->valid && entry->flags == 0x24 && modules.used == image_size);
    entry->numLoaded = 1;
    nano_actor_module_release(ACTOR_EN_KO);
    nano_actor_modules_collect();
    assert(slot.references == 1 && entry->valid);
    entry->numLoaded = 0;
    nano_actor_module_release(ACTOR_EN_KO);
    assert(slot.references == 1);
    nano_actor_modules_collect();
    assert(!slot.references && !entry->valid && !modules.used);
    actors[ACTOR_EN_KO].profile_offset = image_size;
    assert(!ActorDB_Retrieve(ACTOR_EN_KO)->valid && !modules.used);
    actors[ACTOR_EN_KO].profile_offset = 0;
    modules.budget = 0;
    assert(!ActorDB_Retrieve(ACTOR_EN_KO)->valid && !modules.used);
    assert(nano_actor_modules_configure(NULL, NULL, NULL));
    puts("Actor registry -> checked module -> retirement -> release integration passed ASan/UBSan.");
    return 0;
}
