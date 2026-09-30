#include "global.h"
#include "soh/ActorDB.h"
#include "soh/Enhancements/game-interactor/GameInteractor_Hooks.h"
#include "soh/frame_interpolation.h"
#include "nano_pack.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

void nano_actor_db_reset(void);
static void actor_callback(Actor *actor, PlayState *play)
{
    (void)actor; (void)play;
}

/* Synthetic actor definitions let the registry be tested independently of
 * scene loading; the ARM archive separately links the real definitions. */
#define DEFINE_ACTOR(name, ident, alloc) \
    ActorInit name##_InitVars = { .id = ident, .category = 7, .flags = 0x42, \
        .objectId = 3, .instanceSize = sizeof(Actor), .init = actor_callback };
#define DEFINE_ACTOR_INTERNAL DEFINE_ACTOR
#define DEFINE_ACTOR_UNSET(id)
#include "tables/actor_table.h"
#undef DEFINE_ACTOR
#undef DEFINE_ACTOR_INTERNAL
#undef DEFINE_ACTOR_UNSET

static void test_actors(void)
{
    unsigned count = 0;
#define DEFINE_ACTOR(name, ident, alloc) do { \
    ActorDBEntry *entry = ActorDB_Retrieve(ident); \
    assert(entry->valid && entry->id == ident); \
    assert(entry->init == actor_callback && entry->flags == 0x42); \
    assert(entry->objectId == 3 && entry->instanceSize == sizeof(Actor)); \
    assert(ActorDB_RetrieveId(#name) == ident); \
    entry->numLoaded = 2; \
    assert(ActorDB_Retrieve(ident)->numLoaded == 2); count++; \
} while (0);
#define DEFINE_ACTOR_INTERNAL DEFINE_ACTOR
#define DEFINE_ACTOR_UNSET(id) assert(!ActorDB_Retrieve(id)->valid);
#include "tables/actor_table.h"
#undef DEFINE_ACTOR
#undef DEFINE_ACTOR_INTERNAL
#undef DEFINE_ACTOR_UNSET
    assert(!ActorDB_Retrieve(-1)->valid);
    assert(!ActorDB_Retrieve(ACTOR_ID_MAX)->valid);
    assert(ActorDB_RetrieveId(NULL) == -1);
    nano_actor_db_reset();
    assert(ActorDB_Retrieve(ACTOR_PLAYER)->numLoaded == 0);
    printf("%u actor entries and all unset slots passed\n", count);
}

static void test_queues(void)
{
    OSMesgQueue queue;
    OSMesg buffer[3], value = {0};
    unsigned round;
    osCreateMesgQueue(&queue, buffer, 3);
    assert(osRecvMesg(&queue, &value, OS_MESG_NOBLOCK) == -1);
    for (round = 0; round < 100; round++) {
        assert(osSendMesg(&queue, OS_MESG_32(1), 0) == 0);
        assert(osSendMesg(&queue, OS_MESG_32(2), 0) == 0);
        assert(osJamMesg(&queue, OS_MESG_32(3), 0) == 0);
        assert(osJamMesg(&queue, OS_MESG_32(4), 0) == -1);
        assert(osSendMesg(&queue, OS_MESG_32(4), 0) == -1);
        assert(osRecvMesg(&queue, &value, 0) == 0 && value.data32 == 3);
        assert(osRecvMesg(&queue, &value, 0) == 0 && value.data32 == 1);
        assert(osRecvMesg(&queue, &value, 0) == 0 && value.data32 == 2);
    }
    osCreateMesgQueue(&queue, NULL, 0);
    assert(osSendMesg(&queue, OS_MESG_32(1), 0) == -1);
    assert(osJamMesg(&queue, OS_MESG_32(1), 0) == -1);
    osSetEventMesg(OS_NUM_EVENTS, &queue, value);
    assert(GameInteractor_Should(0, 42));
    assert(!GameInteractor_Should(0, 0));
    assert(GameInteractor_ShouldActorUpdate(NULL));
    assert(GameInteractor_MovementSpeedMultiplier() == 1.0f);
    assert(GameInteractor_GravityLevel() == GI_GRAVITY_LEVEL_NORMAL);
    assert(GameInteractor_GetLinkSize() == GI_LINK_SIZE_NORMAL);
    assert(!GameInteractor_NoUIActive());
    assert(!GameInteractor_OneHitKOActive());
    assert(!GameInteractor_GetEmulatedButtons());
    puts("queue wrap/full/empty and vanilla feature semantics passed");
}

struct file_io { const char *root; int damage, truncate; };

static int disk_read(void *context, const char *name, void *dst,
                     uint32_t capacity, uint32_t *got)
{
    struct file_io *io = context;
    char path[1024];
    long size;
    FILE *stream;
    if (snprintf(path, sizeof path, "%s/%s", io->root, name) >= (int)sizeof path)
        return -1;
    stream = fopen(path, "rb");
    if (!stream) return -1;
    if (fseek(stream, 0, SEEK_END) || (size = ftell(stream)) < 0 ||
        (unsigned long)size > capacity || fseek(stream, 0, SEEK_SET)) {
        fclose(stream); return -1;
    }
    *got = fread(dst, 1, (size_t)size, stream);
    fclose(stream);
    if (*got != (uint32_t)size) return -1;
    if (io->damage >= 0 && !strcmp(name, "pack.nsp"))
        ((uint8_t *)dst)[(unsigned)io->damage % *got] ^= 1;
    if (io->truncate >= 0 && !strcmp(name, "pack.nsp"))
        *got = (unsigned)io->truncate;
    return 0;
}

static void test_pack(const char *root)
{
    static struct nano_pack pack;
    struct file_io io = { root, -1, -1 };
    struct nano_resource res;
    uint8_t *out = malloc(131089);
    unsigned i;
    assert(out);
    assert(nano_pack_open(&pack, disk_read, &io) == 0);
    assert(nano_pack_find(&pack, nano_resource_hash("test/cross-page"), &res) == 0);
    assert(res.size == 131089);
    assert(nano_pack_load(&pack, &res, out, 131089) == 0);
    for (i = 0; i < 131089; i++) assert(out[i] == (uint8_t)i);
    assert(nano_pack_read(&pack, &res, UINT32_MAX, out, 100) != 0);
    assert(nano_pack_load(&pack, &res, out, 131088) != 0);
    for (i = 0; i < 60; i++) {
        io.damage = (int)i;
        assert(nano_pack_open(&pack, disk_read, &io) != 0);
    }
    io.damage = -1;
    for (i = 0; i < 60; i++) {
        io.truncate = (int)i;
        assert(nano_pack_open(&pack, disk_read, &io) != 0);
    }
    free(out);
    nano_pack_close(&pack);
    puts("paged C reader, bounds and malformed headers passed");
}

int main(int argc, char **argv)
{
    assert(argc == 2);
    test_actors();
    test_queues();
    test_pack(argv[1]);
    return 0;
}
