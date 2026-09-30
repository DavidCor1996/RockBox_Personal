#include "global.h"
#include "soh/ActorDB.h"
#include "nano_actor_modules.h"
#include <assert.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

void nano_actor_db_reset(void);
static ActorInit *owned;
static unsigned acquired, released;
static int fail, wrong_id;

static void update(Actor *actor, PlayState *play)
{
    (void)actor; (void)play;
}

static const ActorInit *acquire(void *unused, int id, const char *name)
{
    (void)unused;
    assert(!owned && name);
    if (fail) return NULL;
    owned = calloc(1, sizeof *owned);
    assert(owned);
    owned->id = wrong_id ? -1 : id;
    owned->instanceSize = sizeof(Actor);
    owned->update = update;
    acquired++;
    return owned;
}

static void release(void *unused, int id)
{
    (void)unused; (void)id;
    assert(owned);
    free(owned);
    owned = NULL;
    released++;
}

int main(void)
{
    ActorDBEntry *actor;
    unsigned i;
    assert(!ActorDB_Retrieve(ACTOR_EN_KO)->valid);
    assert(!nano_actor_modules_configure(acquire, NULL, NULL));
    assert(nano_actor_modules_configure(acquire, release, NULL));
    fail = 1;
    assert(!ActorDB_Retrieve(ACTOR_EN_KO)->valid && !acquired);
    fail = 0; wrong_id = 1;
    assert(!ActorDB_Retrieve(ACTOR_EN_KO)->valid && acquired == released);
    wrong_id = 0;
    for (i = 0; i < 1000; i++) {
        actor = ActorDB_Retrieve(ACTOR_EN_KO);
        assert(actor->valid && actor->update == update);
        assert(ActorDB_Retrieve(ACTOR_EN_KO) == actor);
        assert(acquired == released + 1);
        actor->numLoaded = 2;
        nano_actor_module_release(ACTOR_EN_KO);
        nano_actor_db_reset();
        assert(actor->numLoaded == 2 && owned);
        assert(!nano_actor_modules_configure(NULL, NULL, NULL));
        actor->numLoaded--;
        nano_actor_module_release(ACTOR_EN_KO);
        assert(actor->numLoaded == 1 && owned);
        actor->numLoaded--;
        nano_actor_module_release(ACTOR_EN_KO);
        assert(actor->valid && owned && acquired == released + 1);
        /* Reacquisition before GPU completion cancels pending retirement. */
        assert(ActorDB_Retrieve(ACTOR_EN_KO) == actor);
        nano_actor_modules_collect();
        assert(actor->valid && owned);
        nano_actor_module_release(ACTOR_EN_KO);
        nano_actor_modules_collect();
        assert(!actor->valid && !actor->update && !owned);
        nano_actor_module_release(ACTOR_EN_KO);
        assert(acquired == released);
    }
    assert(!ActorDB_Retrieve(-1)->valid);
    assert(!ActorDB_Retrieve(ACTOR_UNSET_1)->valid);
    assert(ActorDB_RetrieveId("En_Ko") == ACTOR_EN_KO);
    actor = ActorDB_Retrieve(ACTOR_EN_KO);
    nano_actor_db_reset();
    assert(actor->valid && owned);
    nano_actor_modules_collect();
    assert(!actor->valid && !owned && acquired == released);
    assert(nano_actor_modules_configure(NULL, NULL, NULL));
    puts("Lazy actor acquire/release, live-reference protection, failure rollback and 1000 reloads passed.");
    return 0;
}
